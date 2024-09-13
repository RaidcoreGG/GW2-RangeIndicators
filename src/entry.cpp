#include <Windows.h>
#include <vector>

#include <DirectXMath.h>

#include "nexus/Nexus.h"
#include "mumble/Mumble.h"
#include "imgui/imgui.h"

#include "Version.h"
#include "Remote.h"

#include "Settings.h"
#include "Shared.h"

namespace dx = DirectX;

void AddonLoad(AddonAPI* aApi);
void AddonUnload();
void ProcessKeybinds(const char* aIdentifier);
void OnMumbleIdentityUpdated(void* aEventArgs);
void AddonRender();
void AddonOptions();
void AddonShortcut();

AddonDefinition AddonDef			= {};
HMODULE hSelf						= nullptr;

std::filesystem::path AddonPath;
std::filesystem::path SettingsPath;

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

	APIDefs->AddSimpleShortcut("QAS_RANGEINDICATORS", AddonShortcut);

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

	av_interp.push_back(MumbleLink->AvatarPosition);
	avf_interp.push_back(MumbleLink->AvatarFront);
	if (av_interp.size() < 15) { return; }
	av_interp.erase(av_interp.begin());
	avf_interp.erase(avf_interp.begin());

	if (!Settings::IsVisible) { return; }

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

	if (Settings::IsHitboxVisible)
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

	for (RangeIndicator& ri : Settings::RangeIndicators)
	{
		if (!ri.IsVisible) { continue; }

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
}

void AddonOptions()
{
	if (ImGui::Checkbox("Enabled##Global", &Settings::IsVisible))
	{
		Settings::Settings[IS_VISIBLE] = Settings::IsVisible;
		Settings::Save(SettingsPath);
	}

	ImGui::Separator();

	ImGui::TextDisabled("Hitbox");

	if (ImGui::Checkbox("Enabled##Hitbox", &Settings::IsHitboxVisible))
	{
		Settings::Settings[IS_HITBOX_VISIBLE] = Settings::IsHitboxVisible;
		Settings::Save(SettingsPath);
	}

	if (ImGui::ColorEdit4U32("##Hitbox", &Settings::HitboxRGBA, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel))
	{
		Settings::Settings[HITBOX_RGBA] = Settings::HitboxRGBA;
		Settings::Save(SettingsPath);
	}
	ImGui::SameLine();
	ImGui::Text("Colour");

	ImGui::Separator();

	int indexRemove = -1;

	ImGui::BeginTable("#rangeindicatorslist", 8, ImGuiTableFlags_SizingFixedFit);

	ImGui::TableNextRow();

	ImGui::TableSetColumnIndex(2);
	ImGui::Text("Range");

	ImGui::TableSetColumnIndex(3);
	ImGui::Text("Radius");

	ImGui::TableSetColumnIndex(4);
	ImGui::Text("Vertical Offset");

	ImGui::TableSetColumnIndex(5);
	ImGui::Text("Thickness");

	std::lock_guard<std::mutex> lock(Settings::RangesMutex);
	for (size_t i = 0; i < Settings::RangeIndicators.size(); i++)
	{
		RangeIndicator& ri = Settings::RangeIndicators[i];

		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		if (ImGui::Checkbox(("##Visible" + std::to_string(i)).c_str(), &ri.IsVisible))
		{
			Settings::Settings[RANGE_INDICATORS][i]["IsVisible"] = ri.IsVisible;
			Settings::Save(SettingsPath);
		}

		ImGui::TableSetColumnIndex(1);
		if (ImGui::ColorEdit4U32(("Colour##" + std::to_string(i)).c_str(), &ri.RGBA, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel))
		{
			Settings::Settings[RANGE_INDICATORS][i]["RGBA"] = ri.RGBA;
			Settings::Save(SettingsPath);
		}

		float inputWidth = ImGui::GetWindowContentRegionWidth() / 5;

		ImGui::TableSetColumnIndex(2);
		ImGui::PushItemWidth(inputWidth);
		if (ImGui::InputFloat(("##Radius" + std::to_string(i)).c_str(), &ri.Radius, 1.0f, 1.0f, "%.0f"))
		{
			bool sort = true;
			Settings::Settings[RANGE_INDICATORS][i]["Radius"] = ri.Radius;
			Settings::Save(SettingsPath);
		}

		ImGui::TableSetColumnIndex(3);
		ImGui::PushItemWidth(inputWidth);
		if (ImGui::InputFloat(("##Arc" + std::to_string(i)).c_str(), &ri.Arc, 1.0f, 1.0f, "%.0f"))
		{
			bool sort = true;

			if (ri.Arc < 0) { ri.Arc = 0; }
			if (ri.Arc > 360) { ri.Arc = 360; }

			Settings::Settings[RANGE_INDICATORS][i]["Arc"] = ri.Arc;
			Settings::Save(SettingsPath);
		}

		ImGui::TableSetColumnIndex(4);
		ImGui::PushItemWidth(inputWidth);
		if (ImGui::InputFloat(("##VOffset" + std::to_string(i)).c_str(), &ri.VOffset, 1.0f, 1.0f, "%.0f"))
		{
			Settings::Settings[RANGE_INDICATORS][i]["VOffset"] = ri.VOffset;
			Settings::Save(SettingsPath);
		}

		ImGui::TableSetColumnIndex(5);
		ImGui::PushItemWidth(inputWidth);
		if (ImGui::InputFloat(("##Thickness" + std::to_string(i)).c_str(), &ri.Thickness, 1.0f, 1.0f, "%.0f"))
		{
			if (ri.Thickness < 1) { ri.Thickness = 1; }
			if (ri.Thickness > 25) { ri.Thickness = 25; }

			Settings::Settings[RANGE_INDICATORS][i]["Thickness"] = ri.Thickness;
			Settings::Save(SettingsPath);
		}

		ImGui::TableSetColumnIndex(6);
		if (ImGui::SmallButton(("Remove##" + std::to_string(i)).c_str()))
		{
			indexRemove = i;
		}
	}

	ImGui::EndTable();

	if (indexRemove > -1)
	{
		Settings::RangeIndicators.erase(Settings::RangeIndicators.begin() + indexRemove);
		Settings::Settings[RANGE_INDICATORS].erase(indexRemove);
		Settings::Save(SettingsPath);
	}

	if (ImGui::SmallButton("Add"))
	{
		Settings::RangeIndicators.push_back(RangeIndicator{ 0xFFFFFFFF, 360, true, 0, 1 });
		json jRi{};
		jRi["RGBA"] = 0xFFFFFFFF;
		jRi["Radius"] = 360;
		jRi["IsVisible"] = true;
		jRi["VOffset"] = 0;
		jRi["Thickness"] = 1;
		Settings::Settings[RANGE_INDICATORS].push_back(jRi);
		Settings::Save(SettingsPath);
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

		if (Settings::IsVisible)
		{
			ImGui::Separator();

			if (ImGui::Checkbox("Hitbox", &Settings::IsHitboxVisible))
			{
				Settings::Settings[IS_HITBOX_VISIBLE] = Settings::IsHitboxVisible;
				Settings::Save(SettingsPath);
			}

			std::lock_guard<std::mutex> lock(Settings::RangesMutex);
			for (size_t i = 0; i < Settings::RangeIndicators.size(); i++)
			{
				RangeIndicator& ri = Settings::RangeIndicators[i];

				if (ImGui::Checkbox(std::to_string(static_cast<int>(ri.Radius)).c_str(), &ri.IsVisible))
				{
					Settings::Settings[RANGE_INDICATORS][i]["IsVisible"] = ri.IsVisible;
					Settings::Save(SettingsPath);
				}
			}
		}

		ImGui::EndMenu();
	}
}