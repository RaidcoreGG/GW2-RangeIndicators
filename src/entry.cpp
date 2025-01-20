#include <Windows.h>
#include <vector>
#include <unordered_map>
#include <algorithm>

#include <DirectXMath.h>

#include "nexus/Nexus.h"
#include "mumble/Mumble.h"
#include "imgui/imgui.h"

#include "Version.h"
#include "Remote.h"

#include "Settings.h"
#include "Shared.h"

#include "Specializations.h"

namespace dx = DirectX;

void AddonLoad(AddonAPI* aApi);
void AddonUnload();
void ProcessKeybinds(const char* aIdentifier);
void OnMumbleIdentityUpdated(void* aEventArgs);
void AddonRender();
void AddonOptions();
void AddonShortcut();
void DrawListOfRangeIndicators();
std::vector<std::pair<int, RangeIndicator>> GetSortedIndicators(const std::vector<RangeIndicator>& indicators);

AddonDefinition AddonDef = {};
HMODULE hSelf = nullptr;

std::filesystem::path AddonPath;
std::filesystem::path SettingsPath;

std::string spec;
std::string coreSpec;

std::vector<std::pair<int, RangeIndicator>> cachedSortedIndicators;
bool sortedIndicatorsNeedsUpdate = true;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH: hSelf = hModule; break;
	case DLL_PROCESS_DETACH: break;
	case DLL_THREAD_ATTACH: break;
	case DLL_THREAD_DETACH: break;
	}
	return TRUE;
}

extern "C" __declspec(dllexport) AddonDefinition* GetAddonDef()
{
	AddonDef.Signature = 31;
	AddonDef.APIVersion = NEXUS_API_VERSION;
	AddonDef.Name = "Range Indicators";
	AddonDef.Version.Major = V_MAJOR;
	AddonDef.Version.Minor = V_MINOR;
	AddonDef.Version.Build = V_BUILD;
	AddonDef.Version.Revision = V_REVISION;
	AddonDef.Author = "Raidcore";
	AddonDef.Description = "Shows your hitbox and allows to display custom ranges.";
	AddonDef.Load = AddonLoad;
	AddonDef.Unload = AddonUnload;
	AddonDef.Flags = EAddonFlags_None;

	/* not necessary if hosted on Raidcore, but shown anyway for the example also useful as a backup resource */
	AddonDef.Provider = EUpdateProvider_GitHub;
	AddonDef.UpdateLink = REMOTE_URL;

	return &AddonDef;
}

void AddonLoad(AddonAPI* aApi)
{
	APIDefs = aApi;
	ImGui::SetCurrentContext((ImGuiContext*)APIDefs->ImguiContext);
	ImGui::SetAllocatorFunctions((void* (*)(size_t, void*))APIDefs->ImguiMalloc, (void(*)(void*, void*))APIDefs->ImguiFree); // on imgui 1.80+

	MumbleLink = (Mumble::Data*)APIDefs->GetResource("DL_MUMBLE_LINK");
	NexusLink = (NexusLinkData*)APIDefs->GetResource("DL_NEXUS_LINK");

	APIDefs->SubscribeEvent("EV_MUMBLE_IDENTITY_UPDATED", OnMumbleIdentityUpdated);

	APIDefs->RegisterRender(ERenderType_Render, AddonRender);
	APIDefs->RegisterRender(ERenderType_OptionsRender, AddonOptions);

	if (Settings::ShortcutMenuEnabled)
	{
		APIDefs->AddSimpleShortcut("QAS_RANGEINDICATORS", AddonShortcut);
	}

	APIDefs->RegisterKeybindWithString("KB_RI_TOGGLEVISIBLE", ProcessKeybinds, "(null)");

	AddonPath = APIDefs->GetAddonDirectory("RangeIndicators");
	SettingsPath = APIDefs->GetAddonDirectory("RangeIndicators/settings.json");
	std::filesystem::create_directory(AddonPath);

	Settings::Load(SettingsPath);
}
void AddonUnload()
{
	APIDefs->DeregisterRender(AddonOptions);
	APIDefs->DeregisterRender(AddonRender);

	APIDefs->UnsubscribeEvent("EV_MUMBLE_IDENTITY_UPDATED", OnMumbleIdentityUpdated);

	APIDefs->DeregisterKeybind("KB_RI_TOGGLEVISIBLE");

	MumbleLink = nullptr;
	NexusLink = nullptr;
}

void ProcessKeybinds(const char* aIdentifier)
{
	std::string str = aIdentifier;

	if (str == "KB_RI_TOGGLEVISIBLE")
	{
		Settings::IsVisible = !Settings::IsVisible;
		Settings::Save(SettingsPath);
	}
}

void OnMumbleIdentityUpdated(void* aEventArgs)
{
	MumbleIdentity = (Mumble::Identity*)aEventArgs;
	spec = Specializations::MumbleIdentToSpecString(MumbleIdentity);
	coreSpec = Specializations::EliteSpecToCoreSpec(spec);
	sortedIndicatorsNeedsUpdate = true; // Invalidate cached sorted indicators because the spec has changed
	APIDefs->Log(ELogLevel::ELogLevel_INFO, "RangeIndicators", std::string("MumbleIdentityUpdated: Spec " + spec + ", CoreSpec " + coreSpec).c_str());
}

std::vector<Vector3> av_interp;
std::vector<Vector3> avf_interp;

Vector3 Average(std::vector<Vector3> aVectors)
{
	Vector3 avg{};
	for (size_t i = 0; i < aVectors.size(); i++)
	{
		avg.X += aVectors[i].X;
		avg.Y += aVectors[i].Y;
		avg.Z += aVectors[i].Z;
	}

	avg.X /= aVectors.size();
	avg.Y /= aVectors.size();
	avg.Z /= aVectors.size();

	return avg;
}

struct ProjectionData
{
	Vector3 AgentPosition;
	Vector3 AgentFront;

	dx::XMVECTOR CameraPosition;
	dx::XMVECTOR CameraLookAt;

	dx::XMMATRIX ViewMatrix;
	dx::XMMATRIX ProjectionMatrix;
	dx::XMMATRIX WorldMatrix;
};

bool DepthOK(float& aDepth)
{
	return /*aDepth >= 0.0f && */aDepth <= 1.0f;
}

void DrawCircle(ProjectionData aProjection, ImDrawList* aDrawList, ImColor aColor, float aRadius, float aVOffset, float aArc, float aThickness, bool aShaded = true, bool aShowFlanks = false)
{
	float fRot = atan2f(MumbleLink->CameraFront.X, MumbleLink->CameraFront.Z) * 180.0f / 3.14159f;
	float camRot = fRot;
	if (camRot < 0.0f) { camRot += 360.0f; }
	if (camRot == 0.0f) { camRot = 360.0f; }

	// convert inches to meters
	aRadius *= 2.54f / 100.0f;
	aVOffset *= 2.54f / 100.0f;

	float flankOffset = aArc / 2;

	if (aArc == 360.0f)
	{
		flankOffset = 45.0f;
	}
	else
	{
		aShowFlanks = true;
	}

	ImColor shadowColor = ImColor(0.f, 0.f, 0.f, ((ImVec4)aColor).w);

	float facingRad = atan2f(aProjection.AgentFront.X, aProjection.AgentFront.Z);
	float facingDeg = facingRad * 180.0f / 3.14159f;

	float flankOffsetRad = flankOffset * 3.14159f / 180.0f;

	std::vector<Vector3> circle;
	for (size_t i = 0; i < 200; i++)
	{
		float degRad = i * aArc / 200 * 3.14159f / 180.0f;

		float x = aRadius * sin(degRad - flankOffsetRad + facingRad) + aProjection.AgentPosition.X;
		float z = aRadius * cos(degRad - flankOffsetRad + facingRad) + aProjection.AgentPosition.Z;

		circle.push_back(Vector3{ x, aProjection.AgentPosition.Y + aVOffset, z });
	}

	Vector3 rightFlank;
	Vector3 leftFlank;

	ImDrawList* dl = ImGui::GetBackgroundDrawList();

	std::vector<Vector3> circleProj;

	/* generate circle points */
	for (size_t i = 0; i < circle.size(); i++)
	{
		dx::XMVECTOR point = { circle[i].X, circle[i].Y, circle[i].Z };
		dx::XMVECTOR pointProjected = dx::XMVector3Project(point, 0, 0, NexusLink->Width, NexusLink->Height, 1.0f, 10000.0f, aProjection.ProjectionMatrix, aProjection.ViewMatrix, aProjection.WorldMatrix);

		/*float deltaCamPoint = sqrt((circle[i].X - camera.X) * (circle[i].X - camera.X) +
			(circle[i].Y - camera.Y) * (circle[i].Y - camera.Y) +
			(circle[i].Z - camera.Z) * (circle[i].Z - camera.Z));*/

		dx::XMVECTOR pointTransformed = dx::XMVector3TransformCoord(point, aProjection.WorldMatrix);
		pointTransformed = dx::XMVector3TransformCoord(pointTransformed, aProjection.ViewMatrix);
		pointTransformed = dx::XMVector3TransformCoord(pointTransformed, aProjection.ProjectionMatrix);

		float depth = dx::XMVectorGetZ(pointTransformed);

		bool behindCamera = (depth < 0.0f) || (depth > 1.0f);

		circleProj.push_back(Vector3{ pointProjected.m128_f32[0], pointProjected.m128_f32[1], depth });
	}

	Vector3 origin{};

	/* transform flanks */
	if (aShowFlanks)
	{
		float offsetRF = 0 + flankOffset + facingDeg;
		float offsetLF = 360.0f - flankOffset + facingDeg;

		if (offsetRF > 360.0f) { offsetRF -= 360.0f; }
		if (offsetRF < 0.0f) { offsetRF += 360.0f; }
		if (offsetLF > 360.0f) { offsetLF -= 360.0f; }
		if (offsetLF < 0.0f) { offsetLF += 360.0f; }

		float cosRF = cos(offsetRF * 3.14159f / 180.0f);
		float sinRF = sin(offsetRF * 3.14159f / 180.0f);
		float cosLF = cos(offsetLF * 3.14159f / 180.0f);
		float sinLF = sin(offsetLF * 3.14159f / 180.0f);

		rightFlank = { aRadius * sinRF + aProjection.AgentPosition.X, aProjection.AgentPosition.Y + aVOffset, aRadius * cosRF + aProjection.AgentPosition.Z };
		leftFlank = { aRadius * sinLF + aProjection.AgentPosition.X, aProjection.AgentPosition.Y + aVOffset, aRadius * cosLF + aProjection.AgentPosition.Z };

		/* don't forget these three for facing cone */
		dx::XMVECTOR RFProj = dx::XMVector3Project({ rightFlank.X, rightFlank.Y, rightFlank.Z }, 0, 0, NexusLink->Width, NexusLink->Height, 1.0f, 10000.0f, aProjection.ProjectionMatrix, aProjection.ViewMatrix, aProjection.WorldMatrix);
		dx::XMVECTOR LFProj = dx::XMVector3Project({ leftFlank.X, leftFlank.Y, leftFlank.Z }, 0, 0, NexusLink->Width, NexusLink->Height, 1.0f, 10000.0f, aProjection.ProjectionMatrix, aProjection.ViewMatrix, aProjection.WorldMatrix);
		dx::XMVECTOR originProj = dx::XMVector3Project({ aProjection.AgentPosition.X, aProjection.AgentPosition.Y + aVOffset, aProjection.AgentPosition.Z }, 0, 0, NexusLink->Width, NexusLink->Height, 1.0f, 10000.0f, aProjection.ProjectionMatrix, aProjection.ViewMatrix, aProjection.WorldMatrix);

		dx::XMVECTOR RFTransformed = dx::XMVector3TransformCoord({ rightFlank.X, rightFlank.Y, rightFlank.Z }, aProjection.WorldMatrix);
		RFTransformed = dx::XMVector3TransformCoord(RFTransformed, aProjection.ViewMatrix);
		RFTransformed = dx::XMVector3TransformCoord(RFTransformed, aProjection.ProjectionMatrix);

		dx::XMVECTOR LFTransformed = dx::XMVector3TransformCoord({ leftFlank.X, leftFlank.Y, leftFlank.Z }, aProjection.WorldMatrix);
		LFTransformed = dx::XMVector3TransformCoord(LFTransformed, aProjection.ViewMatrix);
		LFTransformed = dx::XMVector3TransformCoord(LFTransformed, aProjection.ProjectionMatrix);

		dx::XMVECTOR originTransformed = dx::XMVector3TransformCoord({ aProjection.AgentPosition.X, aProjection.AgentPosition.Y, aProjection.AgentPosition.Z }, aProjection.WorldMatrix);
		originTransformed = dx::XMVector3TransformCoord(originTransformed, aProjection.ViewMatrix);
		originTransformed = dx::XMVector3TransformCoord(originTransformed, aProjection.ProjectionMatrix);

		rightFlank = Vector3{ RFProj.m128_f32[0], RFProj.m128_f32[1], dx::XMVectorGetZ(RFTransformed) };
		leftFlank = Vector3{ LFProj.m128_f32[0], LFProj.m128_f32[1], dx::XMVectorGetZ(LFTransformed) };
		origin = Vector3{ originProj.m128_f32[0], originProj.m128_f32[1], dx::XMVectorGetZ(originTransformed) };
	}

	/* pass for the "shadows" */
	if (aShaded)
	{
		for (size_t i = 0; i < circleProj.size(); i++)
		{
			if (i > 0)
			{
				Vector3& p1 = circleProj[i - 1];
				Vector3& p2 = circleProj[i];
				if (DepthOK(p1.Z) && DepthOK(p2.Z))
					dl->AddLine(ImVec2(p1.X + 1.0f, p1.Y + 1.0f), ImVec2(p2.X + 1.0f, p2.Y + 1.0f), shadowColor, aThickness);
			}
		}

		if (aArc == 360.0f)
		{
			Vector3& p1 = circleProj[circleProj.size() - 1];
			Vector3& p2 = circleProj[0];
			if (DepthOK(p1.Z) && DepthOK(p2.Z))
				dl->AddLine(ImVec2(p1.X + 1.0f, p1.Y + 1.0f), ImVec2(p2.X + 1.0f, p2.Y + 1.0f), shadowColor, aThickness);
		}
		else /*if (aShowFlanks)*/
		{
			Vector3& p1 = circleProj[circleProj.size() - 1];
			Vector3& p2 = circleProj[0];
			if (DepthOK(p1.Z) && DepthOK(p2.Z))
				dl->AddLine(ImVec2(p1.X + 1.0f, p1.Y + 1.0f), ImVec2(rightFlank.X + 1.0f, rightFlank.Y + 1.0f), shadowColor, aThickness);
		}

		if (aShowFlanks)
		{
			if (DepthOK(rightFlank.Z) && DepthOK(origin.Z))
				dl->AddLine(ImVec2(rightFlank.X + 1.0f, rightFlank.Y + 1.0f), ImVec2(origin.X + 1.0f, origin.Y + 1.0f), shadowColor, aThickness);
			if (DepthOK(leftFlank.Z) && DepthOK(origin.Z))
				dl->AddLine(ImVec2(leftFlank.X + 1.0f, leftFlank.Y + 1.0f), ImVec2(origin.X + 1.0f, origin.Y + 1.0f), shadowColor, aThickness);
		}
	}

	/* (maybe second) pass for the actual lines */
	for (size_t i = 0; i < circleProj.size(); i++)
	{
		if (i > 0)
		{
			Vector3& p1 = circleProj[i - 1];
			Vector3& p2 = circleProj[i];
			if (DepthOK(p1.Z) && DepthOK(p2.Z))
				dl->AddLine(ImVec2(p1.X, p1.Y), ImVec2(p2.X, p2.Y), aColor, aThickness);
		}
	}

	if (aArc == 360.0f)
	{
		Vector3& p1 = circleProj[circleProj.size() - 1];
		Vector3& p2 = circleProj[0];
		if (DepthOK(p1.Z) && DepthOK(p2.Z))
			dl->AddLine(ImVec2(p1.X, p1.Y), ImVec2(p2.X, p2.Y), aColor, aThickness);
	}
	else /*if (aShowFlanks)*/
	{
		Vector3& p1 = circleProj[circleProj.size() - 1];
		Vector3& p2 = circleProj[0];
		if (DepthOK(p1.Z) && DepthOK(p2.Z))
			dl->AddLine(ImVec2(p1.X, p1.Y), ImVec2(rightFlank.X, rightFlank.Y), aColor, aThickness);
	}

	if (aShowFlanks)
	{
		if (DepthOK(rightFlank.Z) && DepthOK(origin.Z))
			dl->AddLine(ImVec2(rightFlank.X, rightFlank.Y), ImVec2(origin.X, origin.Y), aColor, aThickness);
		if (DepthOK(leftFlank.Z) && DepthOK(origin.Z))
			dl->AddLine(ImVec2(leftFlank.X, leftFlank.Y), ImVec2(origin.X, origin.Y), aColor, aThickness);
	}
}

void AddonRender()
{
	if (!NexusLink || !MumbleLink || !MumbleIdentity || MumbleLink->Context.IsMapOpen || !NexusLink->IsGameplay) { return; }

	if (!Settings::IsVisible) { return; }

	av_interp.push_back(MumbleLink->AvatarPosition);
	avf_interp.push_back(MumbleLink->AvatarFront);
	if (av_interp.size() < 15) { return; }
	av_interp.erase(av_interp.begin());
	avf_interp.erase(avf_interp.begin());

	dx::XMVECTOR camPos = { MumbleLink->CameraPosition.X, MumbleLink->CameraPosition.Y, MumbleLink->CameraPosition.Z };

	dx::XMVECTOR lookAtPosition = dx::XMVectorAdd(camPos, { MumbleLink->CameraFront.X, MumbleLink->CameraFront.Y, MumbleLink->CameraFront.Z });

	ProjectionData projectionCtx
	{
		Average(av_interp),
		Average(avf_interp),

		camPos,
		lookAtPosition,

		dx::XMMatrixLookAtLH(camPos, lookAtPosition, { 0, 1.0f, 0 }),
		dx::XMMatrixPerspectiveFovLH(MumbleIdentity->FOV, (float)NexusLink->Width / (float)NexusLink->Height, 1.0f, 10000.0f),
		dx::XMMatrixIdentity()
	};

	ImDrawList* dl = ImGui::GetBackgroundDrawList();

	if (Settings::IsHitboxVisible && (!Settings::InCombatOnly || Settings::AlwaysShowHitbox || MumbleLink->Context.IsInCombat))
	{
		float radius = 24.0f; // normal player
		switch (MumbleLink->Context.MountIndex)
		{
		case Mumble::EMountIndex::Raptor:
		case Mumble::EMountIndex::Griffon:
		case Mumble::EMountIndex::RollerBeetle:
		case Mumble::EMountIndex::Skyscale:
			radius = 60.0f;
			break;

		case Mumble::EMountIndex::Springer:
		case Mumble::EMountIndex::Jackal:
			radius = 50.0f;
			break;

		case Mumble::EMountIndex::Skimmer:
			radius = 66.0f;
			break;

		case Mumble::EMountIndex::Warclaw:
			radius = 40.0f;
			break;

		case Mumble::EMountIndex::SiegeTurtle:
			radius = 80.0f;
			break;
		}
		DrawCircle(projectionCtx, dl, Settings::HitboxRGBA, radius, 0, 360, 1, true, true);
	}

	if (Settings::InCombatOnly && !MumbleLink->Context.IsInCombat) { return; }

	for (RangeIndicator& ri : Settings::RangeIndicators)
	{
		if (!ri.IsVisible) { continue; }
		// Skip if filtering is enabled and spec doesn't match
		if (Settings::FilterSpecialization && ri.Specialization != spec) {
			if (!(Settings::FilterProfession && ri.Specialization == coreSpec)) {
				if (ri.Specialization != "ALL" && !ri.Specialization.empty()) {
					continue;
				}
			}
		}

		DrawCircle(projectionCtx, dl, ri.RGBA, ri.Radius, ri.VOffset, ri.Arc, ri.Thickness, true, false);
	}
}

namespace ImGui
{
	bool ColorEdit4U32(const char* label, ImU32* color, ImGuiColorEditFlags flags = 0) {
		float col[4];
		col[0] = (float)((*color >> 0) & 0xFF) / 255.0f;
		col[1] = (float)((*color >> 8) & 0xFF) / 255.0f;
		col[2] = (float)((*color >> 16) & 0xFF) / 255.0f;
		col[3] = (float)((*color >> 24) & 0xFF) / 255.0f;

		bool result = ColorEdit4(label, col, flags);

		*color = ((ImU32)(col[0] * 255.0f)) |
			((ImU32)(col[1] * 255.0f) << 8) |
			((ImU32)(col[2] * 255.0f) << 16) |
			((ImU32)(col[3] * 255.0f) << 24);

		return result;
	}

	void ShowDelayedTooltipOnHover(const char* tooltip, float delay)
	{
		/* This is a workaround to fix the tooltip not showing when hovering over the same item multiple times
		 * This could also be fixed by upgrading to a newer version of ImGui that supports the new flags for ImGui::SetTooltip
		 */

		 // Use a static map to track hover start times for different tooltips
		static std::unordered_map<const char*, double> hoverStartTimes;

		if (ImGui::IsItemHovered()) {
			// Initialize hover start time if not already set for this tooltip
			if (hoverStartTimes.find(tooltip) == hoverStartTimes.end()) {
				hoverStartTimes[tooltip] = ImGui::GetTime();
			}

			// Show tooltip if enough time has elapsed
			if (ImGui::GetTime() - hoverStartTimes[tooltip] >= delay) {
				ImGui::SetTooltip("%s", tooltip);
			}
		}
		else {
			// Reset hover start time when no longer hovering
			hoverStartTimes.erase(tooltip);
		}
	}
}

void AddonOptions()
{
	if (ImGui::Checkbox("Enabled##Global", &Settings::IsVisible))
	{
		Settings::Settings[IS_VISIBLE] = Settings::IsVisible;
		Settings::Save(SettingsPath);
	}

	if (ImGui::Checkbox("Only show in combat##Global", &Settings::InCombatOnly))
	{
		Settings::Settings[IN_COMBAT_ONLY] = Settings::InCombatOnly;
		Settings::Save(SettingsPath);
	}

	ImGui::Separator();

	ImGui::TextDisabled("Hitbox");

	if (ImGui::Checkbox("Enabled##Hitbox", &Settings::IsHitboxVisible))
	{
		Settings::Settings[IS_HITBOX_VISIBLE] = Settings::IsHitboxVisible;
		Settings::Save(SettingsPath);
	}

	if (Settings::IsHitboxVisible && Settings::InCombatOnly)
	{
		ImGui::SameLine();
		if (ImGui::Checkbox("Always show hitbox##Hitbox", &Settings::AlwaysShowHitbox))
		{
			Settings::Settings[ALWAYS_SHOW_HITBOX] = Settings::AlwaysShowHitbox;
			Settings::Save(SettingsPath);
		}
		ImGui::ShowDelayedTooltipOnHover(
			"Show hitbox even when not in combat, this setting is only used when 'Only show in combat' is enabled",
			1.0f);
	}

	if (ImGui::ColorEdit4U32("##Hitbox", &Settings::HitboxRGBA, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel))
	{
		Settings::Settings[HITBOX_RGBA] = Settings::HitboxRGBA;
		Settings::Save(SettingsPath);
	}
	ImGui::SameLine();
	ImGui::Text("Colour");

	ImGui::Separator();

	ImGui::TextDisabled("Specializations");

	if (ImGui::Checkbox("Filter specialization##Specializations", &Settings::FilterSpecialization))
	{
		Settings::Settings[FILTER_SPECIALIZATION] = Settings::FilterSpecialization;
		Settings::Save(SettingsPath);
	}
	ImGui::ShowDelayedTooltipOnHover("Only show range indicators for your current specialization", 1.0f);

	if (Settings::FilterSpecialization)
	{
		ImGui::SameLine();
		if (ImGui::Checkbox("Show core on all specs##Specializations", &Settings::FilterProfession))
		{
			Settings::Settings[FILTER_PROFESSION] = Settings::FilterProfession;
			Settings::Save(SettingsPath);
		}
		ImGui::ShowDelayedTooltipOnHover("Show range indicators for your core profession on all specs", 1.0f);
		ImGui::SameLine();
		if (ImGui::Checkbox("Sort list by profession##Specializations", &Settings::SortByProfession))
		{
			Settings::Settings[SORT_BY_PROFESSION] = Settings::SortByProfession;
			Settings::Save(SettingsPath);
		}
		ImGui::ShowDelayedTooltipOnHover("Sort range indicators by profession", 1.0f);
	}

	ImGui::Separator();

	ImGui::TextDisabled("Shortcut Menu");

	if (ImGui::Checkbox("Enabled##Shortcuts", &Settings::ShortcutMenuEnabled))
	{
		Settings::Settings[SHORTCUT_MENU_ENABLED] = Settings::ShortcutMenuEnabled;
		Settings::Save(SettingsPath);

		if (!Settings::ShortcutMenuEnabled)
		{
			APIDefs->RemoveSimpleShortcut("QAS_RANGEINDICATORS");
		}
		if (Settings::ShortcutMenuEnabled)
		{
			APIDefs->AddSimpleShortcut("QAS_RANGEINDICATORS", AddonShortcut);
		}
	}
	ImGui::ShowDelayedTooltipOnHover("Enable the shortcut menu", 1.0f);

	if (Settings::ShortcutMenuEnabled)
	{
		if (ImGui::Checkbox("Hitbox Toggle##Shortcuts", &Settings::HitboxToggle))
		{
			Settings::Settings[SHORTCUT_HITBOX_TOGGLE] = Settings::HitboxToggle;
			Settings::Save(SettingsPath);
		}
		ImGui::ShowDelayedTooltipOnHover("Put hitbox toggle in the shortcut menu", 1.0f);

		if (ImGui::Checkbox("Combat Toggle##Shortcuts", &Settings::CombatToggle))
		{
			Settings::Settings[SHORTCUT_COMBAT_TOGGLE] = Settings::CombatToggle;
			Settings::Save(SettingsPath);
		}
		ImGui::ShowDelayedTooltipOnHover("Put only show in combat toggle in the shortcut menu", 1.0f);

		if (ImGui::Checkbox("Always show hitbox Toggle##Shortcuts", &Settings::AlwaysShowHitboxToggle))
		{
			Settings::Settings[SHORTCUT_ALWAYS_SHOW_HITBOX_TOGGLE] = Settings::AlwaysShowHitboxToggle;
			Settings::Save(SettingsPath);
		}
		ImGui::ShowDelayedTooltipOnHover("Put always show hitbox toggle in the shortcut menu", 1.0f);

		if (ImGui::Checkbox("Filter specialization Toggle##Shortcuts", &Settings::FilterSpecializationToggle))
		{
			Settings::Settings[SHORTCUT_FILTER_SPECIALIZATION_TOGGLE] = Settings::FilterSpecializationToggle;
			Settings::Save(SettingsPath);
		}
		ImGui::ShowDelayedTooltipOnHover("Put filter specialization toggle in the shortcut menu", 1.0f);

		if (ImGui::Checkbox("Show core on all specs Toggle##Shortcuts", &Settings::FilterProfessionToggle))
		{
			Settings::Settings[SHORTCUT_FILTER_PROFESSION_TOGGLE] = Settings::FilterProfessionToggle;
			Settings::Save(SettingsPath);
		}
		ImGui::ShowDelayedTooltipOnHover("Put show core on all specs toggle in the shortcut menu", 1.0f);

		if (ImGui::Checkbox("Sort list by profession Toggle##Shortcuts", &Settings::SortByProfessionToggle))
		{
			Settings::Settings[SHORTCUT_SORT_BY_PROFESSION_TOGGLE] = Settings::SortByProfessionToggle;
			Settings::Save(SettingsPath);
		}
		ImGui::ShowDelayedTooltipOnHover("Put sort list by profession toggle in the shortcut menu", 1.0f);
	}

	ImGui::Separator();

	ImGui::TextDisabled("Range Indicators");

	DrawListOfRangeIndicators();
}

void DrawListOfRangeIndicators()
{
	int indexRemove = -1;
	struct EditInfo {
		int index;
		RangeIndicator indicator;
		bool needsSave;
	};
	EditInfo editInfo = { -1, {}, false };

	// Create a sorted copy of the range indicators if sorting is enabled
	if (Settings::SortByProfession && sortedIndicatorsNeedsUpdate) {
		cachedSortedIndicators = GetSortedIndicators(Settings::RangeIndicators);
		sortedIndicatorsNeedsUpdate = false;
	}

	ImGui::BeginTable("#rangeindicatorslist", 9, ImGuiTableFlags_SizingFixedFit);

	ImGui::TableNextRow();

	ImGui::TableSetColumnIndex(2);
	ImGui::Text("Name");

	ImGui::TableSetColumnIndex(3);
	ImGui::Text("Radius");

	ImGui::TableSetColumnIndex(4);
	ImGui::Text("Arc");

	ImGui::TableSetColumnIndex(5);
	ImGui::Text("Vert. Offset");

	ImGui::TableSetColumnIndex(6);
	ImGui::Text("Thickness");

	ImGui::TableSetColumnIndex(7);
	ImGui::Text("Specialization");

	std::lock_guard<std::mutex> lock(Settings::RangesMutex);

	const int numIndicators = (int)Settings::RangeIndicators.size();
	std::string lastCore = "";

	for (int i = 0; i < numIndicators; i++)
	{
		// Get the correct indicator and index based on whether we're sorting
		int originalIndex;
		RangeIndicator& ri = Settings::SortByProfession ?
			(originalIndex = cachedSortedIndicators[i].first, cachedSortedIndicators[i].second) :
			(originalIndex = i, Settings::RangeIndicators[i]);

		// Add separator and header between professions when sorting
		if (Settings::SortByProfession) {
			std::string currentCore = ri.Specialization;
			bool isGeneral = ri.Specialization == "ALL" || ri.Specialization.empty();

			if (!isGeneral) {
				if (Specializations::EliteSpecToCoreSpec(ri.Specialization) != "Unknown") {
					currentCore = Specializations::EliteSpecToCoreSpec(ri.Specialization);
				}

				if (currentCore != lastCore) {
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(7);
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
					ImGui::Text("%s", currentCore.c_str());
					ImGui::PopStyleColor();
				}
				lastCore = currentCore;
			}
			else if (lastCore != "") {
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(7);
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
				ImGui::Text("General");
				ImGui::PopStyleColor();
				lastCore = "";
			}
		}

		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		if (ImGui::Checkbox(("##Visible" + std::to_string(originalIndex)).c_str(), &ri.IsVisible))
		{
			editInfo = { originalIndex, ri, true };
		}
		float checkboxWidth = ImGui::CalcItemWidth();

		ImGui::TableSetColumnIndex(1);
		if (ImGui::ColorEdit4U32(("Colour##" + std::to_string(originalIndex)).c_str(), &ri.RGBA, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel))
		{
			editInfo = { originalIndex, ri, true };
		}
		float colorEditWidth = ImGui::CalcItemWidth();

		// Calculate remove button width first
		float removeButtonWidth = ImGui::CalcTextSize("Remove").x + ImGui::GetStyle().FramePadding.x * 4;
		// Calculate remaining width for other columns
		float remainingWidth = ImGui::GetWindowContentRegionWidth() - checkboxWidth - colorEditWidth - removeButtonWidth;
		float inputWidth = remainingWidth / 6; // 6 columns (Name, Radius, Arc, VOffset, Thickness, Spec)

		// Use these widths for the input columns...

		ImGui::TableSetColumnIndex(2);
		ImGui::PushItemWidth(inputWidth);
		char nameBuf[MAX_NAME_LENGTH + 1];  // +1 for null terminator
		strncpy_s(nameBuf, ri.Name.c_str(), MAX_NAME_LENGTH);
		if (ImGui::InputText(("##Name" + std::to_string(originalIndex)).c_str(), nameBuf, sizeof(nameBuf)))
		{
			ri.Name = nameBuf;
			editInfo = { originalIndex, ri, true };
		}

		ImGui::TableSetColumnIndex(3);
		ImGui::PushItemWidth(inputWidth);
		if (ImGui::InputFloat(("##Radius" + std::to_string(originalIndex)).c_str(), &ri.Radius, 1.0f, 1.0f, "%.0f"))
		{
			editInfo = { originalIndex, ri, true };
		}

		ImGui::TableSetColumnIndex(4);
		ImGui::PushItemWidth(inputWidth);
		if (ImGui::InputFloat(("##Arc" + std::to_string(originalIndex)).c_str(), &ri.Arc, 1.0f, 1.0f, "%.0f"))
		{
			if (ri.Arc < 0) { ri.Arc = 0; }
			if (ri.Arc > 360) { ri.Arc = 360; }
			editInfo = { originalIndex, ri, true };
		}

		ImGui::TableSetColumnIndex(5);
		ImGui::PushItemWidth(inputWidth * 0.75f);
		if (ImGui::InputFloat(("##VOffset" + std::to_string(originalIndex)).c_str(), &ri.VOffset, 1.0f, 1.0f, "%.0f"))
		{
			editInfo = { originalIndex, ri, true };
		}

		ImGui::TableSetColumnIndex(6);
		ImGui::PushItemWidth(inputWidth * 0.75f);
		if (ImGui::InputFloat(("##Thickness" + std::to_string(originalIndex)).c_str(), &ri.Thickness, 1.0f, 1.0f, "%.0f"))
		{
			if (ri.Thickness < 1) { ri.Thickness = 1; }
			if (ri.Thickness > 25) { ri.Thickness = 25; }
			editInfo = { originalIndex, ri, true };
		}

		ImGui::TableSetColumnIndex(7);
		ImGui::PushItemWidth(inputWidth);
		if (ImGui::BeginCombo(("##Specialization" + std::to_string(originalIndex)).c_str(), ri.Specialization.c_str()))
		{
			std::vector<std::string> specs = Specializations::distinctSpecializationNames;
			specs.insert(specs.begin(), "ALL");
			for (const std::string& spec : specs)
			{
				if (ImGui::Selectable(spec.c_str()))
				{
					ri.Specialization = spec;
					editInfo = { originalIndex, ri, true };
				}
			}
			ImGui::EndCombo();
		}

		ImGui::TableSetColumnIndex(8);
		ImGui::PushItemWidth(removeButtonWidth);
		if (ImGui::SmallButton(("Remove##" + std::to_string(originalIndex)).c_str()))
		{
			indexRemove = originalIndex;
		}
	}

	ImGui::EndTable();

	// Handle removal
	if (indexRemove > -1)
	{
		Settings::RangeIndicators.erase(Settings::RangeIndicators.begin() + indexRemove);
		Settings::Settings[RANGE_INDICATORS].erase(indexRemove);
		Settings::Save(SettingsPath);
		sortedIndicatorsNeedsUpdate = true;
	}

	// Handle edit
	if (editInfo.needsSave)
	{
		Settings::RangeIndicators[editInfo.index] = editInfo.indicator;
		json& jRi = Settings::Settings[RANGE_INDICATORS][editInfo.index];
		jRi["RGBA"] = editInfo.indicator.RGBA;
		jRi["Name"] = editInfo.indicator.Name;
		jRi["Radius"] = editInfo.indicator.Radius;
		jRi["Arc"] = editInfo.indicator.Arc;
		jRi["IsVisible"] = editInfo.indicator.IsVisible;
		jRi["VOffset"] = editInfo.indicator.VOffset;
		jRi["Thickness"] = editInfo.indicator.Thickness;
		jRi["Specialization"] = editInfo.indicator.Specialization;
		Settings::Save(SettingsPath);
		sortedIndicatorsNeedsUpdate = true;
	}

	if (ImGui::SmallButton("Add"))
	{
		RangeIndicator ri{ 0xFFFFFFFF, 360, true, 0, 360, 1, "ALL", "" };
		Settings::RangeIndicators.push_back(ri);
		json jRi{};
		jRi["RGBA"] = ri.RGBA;
		jRi["Name"] = ri.Name;
		jRi["Radius"] = ri.Radius;
		jRi["Arc"] = ri.Arc;
		jRi["IsVisible"] = ri.IsVisible;
		jRi["VOffset"] = ri.VOffset;
		jRi["Thickness"] = ri.Thickness;
		jRi["Specialization"] = ri.Specialization;
		Settings::Settings[RANGE_INDICATORS].push_back(jRi);
		Settings::Save(SettingsPath);
		sortedIndicatorsNeedsUpdate = true;
	}
}

void AddonShortcut()
{
	ImGui::SameLine();
	if (ImGui::BeginMenu("##"))
	{
		if (ImGui::Checkbox("Enabled##Global", &Settings::IsVisible))
		{
			Settings::Settings[IS_VISIBLE] = Settings::IsVisible;
			Settings::Save(SettingsPath);
		}
		if (Settings::CombatToggle && ImGui::Checkbox("Only show in combat##Global", &Settings::InCombatOnly))
		{
			Settings::Settings[IN_COMBAT_ONLY] = Settings::InCombatOnly;
			Settings::Save(SettingsPath);
		}

		if (Settings::IsVisible)
		{
			ImGui::Separator();

			if (Settings::HitboxToggle && ImGui::Checkbox("Hitbox##Hitbox", &Settings::IsHitboxVisible))
			{
				Settings::Settings[IS_HITBOX_VISIBLE] = Settings::IsHitboxVisible;
				Settings::Save(SettingsPath);
			}

			if (Settings::AlwaysShowHitboxToggle && Settings::IsHitboxVisible && Settings::InCombatOnly)
			{
				ImGui::SameLine();
				if (ImGui::Checkbox("Always show##Hitbox", &Settings::AlwaysShowHitbox))
				{
					Settings::Settings[ALWAYS_SHOW_HITBOX] = Settings::AlwaysShowHitbox;
					Settings::Save(SettingsPath);
				}
			}

			ImGui::Separator();

			if (Settings::FilterSpecializationToggle && ImGui::Checkbox("Filter Spec##Specializations", &Settings::FilterSpecialization))
			{
				Settings::Settings[FILTER_SPECIALIZATION] = Settings::FilterSpecialization;
				Settings::Save(SettingsPath);
			}

			if (Settings::FilterSpecialization)
			{
				ImGui::SameLine();
				if (Settings::FilterProfessionToggle && ImGui::Checkbox("Show All##Specializations", &Settings::FilterProfession))
				{
					Settings::Settings[FILTER_PROFESSION] = Settings::FilterProfession;
					Settings::Save(SettingsPath);
				}
				ImGui::SameLine();
				if (Settings::SortByProfessionToggle && ImGui::Checkbox("Sort List##Specializations", &Settings::SortByProfession))
				{
					Settings::Settings[SORT_BY_PROFESSION] = Settings::SortByProfession;
					Settings::Save(SettingsPath);
				}
			}

			ImGui::Separator();

			std::lock_guard<std::mutex> lock(Settings::RangesMutex);

			if (Settings::SortByProfession) {
				if (sortedIndicatorsNeedsUpdate) {
					cachedSortedIndicators = GetSortedIndicators(Settings::RangeIndicators);
					sortedIndicatorsNeedsUpdate = false;
				}
				for (const auto& [originalIndex, ri] : cachedSortedIndicators)
				{
					// Skip if filtering is enabled and spec doesn't match
					if (Settings::FilterSpecialization && ri.Specialization != spec) {
						if (!(Settings::FilterProfession && ri.Specialization == coreSpec)) {
							if (ri.Specialization != "ALL" && !ri.Specialization.empty()) {
								continue;
							}
						}
					}

					// Create the label text
					std::string label;
					if (!ri.Name.empty()) {
						label = ri.Name + " (" + std::to_string(static_cast<int>(ri.Radius));
						if (ri.Arc != 360) {
							label += "/" + std::to_string(static_cast<int>(ri.Arc));
						}
						label += ")";
					}
					else {
						label = ri.Specialization + " (" + std::to_string(static_cast<int>(ri.Radius));
						if (ri.Arc != 360) {
							label += "/" + std::to_string(static_cast<int>(ri.Arc));
						}
						label += ")";
					}

					// Draw colored square
					float squareSize = ImGui::GetFrameHeight();  // Match checkbox size
					ImVec2 pos = ImGui::GetCursorScreenPos();
					ImGui::Dummy(ImVec2(squareSize, squareSize));  // Reserve space
					ImGui::GetWindowDrawList()->AddRectFilled(
						pos,
						ImVec2(pos.x + squareSize, pos.y + squareSize),
						ri.RGBA
					);
					ImGui::SameLine();

					// Checkbox with label
					if (ImGui::Checkbox((label + "##" + std::to_string(originalIndex)).c_str(),
						&Settings::RangeIndicators[originalIndex].IsVisible))
					{
						Settings::Settings[RANGE_INDICATORS][originalIndex]["IsVisible"] = Settings::RangeIndicators[originalIndex].IsVisible;
						Settings::Save(SettingsPath);
					}
				}
			}
			else {
				for (size_t i = 0; i < Settings::RangeIndicators.size(); i++)
				{
					RangeIndicator& ri = Settings::RangeIndicators[i];

					// Skip if filtering is enabled and spec doesn't match
					if (Settings::FilterSpecialization && ri.Specialization != spec) {
						if (!(Settings::FilterProfession && ri.Specialization == coreSpec)) {
							if (ri.Specialization != "ALL" && !ri.Specialization.empty()) {
								continue;
							}
						}
					}

					// Create the label text
					std::string label;
					if (!ri.Name.empty()) {
						label = ri.Name + " (" + std::to_string(static_cast<int>(ri.Radius));
						if (ri.Arc != 360) {
							label += "/" + std::to_string(static_cast<int>(ri.Arc));
						}
						label += ")";
					}
					else {
						label = ri.Specialization + " (" + std::to_string(static_cast<int>(ri.Radius));
						if (ri.Arc != 360) {
							label += "/" + std::to_string(static_cast<int>(ri.Arc));
						}
						label += ")";
					}

					// Draw colored square
					float squareSize = ImGui::GetFrameHeight();  // Match checkbox size
					ImVec2 pos = ImGui::GetCursorScreenPos();
					ImGui::Dummy(ImVec2(squareSize, squareSize));  // Reserve space
					ImGui::GetWindowDrawList()->AddRectFilled(
						pos,
						ImVec2(pos.x + squareSize, pos.y + squareSize),
						ri.RGBA
					);
					ImGui::SameLine();

					// Checkbox with label
					if (ImGui::Checkbox((label + "##" + std::to_string(i)).c_str(),
						&ri.IsVisible))
					{
						Settings::Settings[RANGE_INDICATORS][i]["IsVisible"] = ri.IsVisible;
						Settings::Save(SettingsPath);
					}
				}
			}
		}

		ImGui::EndMenu();
	}
}

std::vector<std::pair<int, RangeIndicator>> GetSortedIndicators(const std::vector<RangeIndicator>& indicators)
{
	std::vector<std::pair<int, RangeIndicator>> sortedIndicators;

	// Create pairs of original index and indicator
	for (int i = 0; i < (int)indicators.size(); i++) {
		sortedIndicators.push_back({ i, indicators[i] });
	}

	// Sort the indicators
	std::sort(sortedIndicators.begin(), sortedIndicators.end(),
		[](const auto& a, const auto& b) {
			const auto& ri1 = a.second;
			const auto& ri2 = b.second;

			// Put "ALL" and empty strings at the very bottom
			bool isGeneral1 = ri1.Specialization == "ALL" || ri1.Specialization.empty();
			bool isGeneral2 = ri2.Specialization == "ALL" || ri2.Specialization.empty();
			if (isGeneral1 != isGeneral2) return isGeneral2;  // Put generals at the bottom
			if (isGeneral1 && isGeneral2) return ri1.Radius < ri2.Radius;  // Sort generals by radius

			// Get core specs
			std::string core1 = ri1.Specialization;
			std::string core2 = ri2.Specialization;
			if (Specializations::EliteSpecToCoreSpec(ri1.Specialization) != "Unknown") {
				core1 = Specializations::EliteSpecToCoreSpec(ri1.Specialization);
			}
			if (Specializations::EliteSpecToCoreSpec(ri2.Specialization) != "Unknown") {
				core2 = Specializations::EliteSpecToCoreSpec(ri2.Specialization);
			}

			// Put current profession's indicators at the top
			if (core1 == coreSpec && core2 != coreSpec) return true;
			if (core2 == coreSpec && core1 != coreSpec) return false;

			// If different professions, sort by profession
			if (core1 != core2) return core1 < core2;

			// Within same profession, sort by specialization
			if (ri1.Specialization != ri2.Specialization)
				return ri1.Specialization < ri2.Specialization;

			// Within same specialization, sort by radius
			if (ri1.Radius != ri2.Radius)
				return ri1.Radius < ri2.Radius;

			// If same radius, sort by arc
			return ri1.Arc < ri2.Arc;
		});

	return sortedIndicators;
}