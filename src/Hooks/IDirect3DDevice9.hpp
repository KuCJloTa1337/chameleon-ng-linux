#pragma once

#include <d3d9.h>

// Function signature of IDirect3DDevice9::EndScene.
typedef HRESULT(__stdcall *EndScene_t) (IDirect3DDevice9*);

// Function signature of IDirect3DDevice9::Reset.
typedef HRESULT(__stdcall *Reset_t) (IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);

// Replacement function that will get called when the device is reset.
HRESULT __stdcall hkReset(IDirect3DDevice9* thisptr, D3DPRESENT_PARAMETERS* params) {
	// Get the original function and store it in a static variable for later usage.
	static Reset_t oReset = d3d9_hook->GetOriginalFunction<Reset_t>(16);
	
	// Nothing special to do until our renderer is ready.
	if (!renderer.IsReady())
		return oReset(thisptr, params);

	// Destroy any ImGui related resources..
	ImGui_ImplDX9_InvalidateDeviceObjects();
	
	// ..call the original 'Reset' function..
	HRESULT result = oReset(thisptr, params);
	
	// ..then recreate them.
	ImGui_ImplDX9_CreateDeviceObjects();

	return result;
}

// Replacement function that will get called before the scene is rendered.
HRESULT __stdcall hkEndScene(IDirect3DDevice9* thisptr) {
	// Get the original function and store it in a static variable for later usage.
	static EndScene_t oEndScene = d3d9_hook->GetOriginalFunction<EndScene_t>(42);

	// Determines whether the mouse is enabled in-game.
	static bool mouse_enabled = false;

	// Check whether the GUI is currently open.
	bool is_renderer_active = renderer.IsActive();

	// TODO: Directly set the value using the ConVar pointer.
	if (is_renderer_active) {
		if (mouse_enabled) {
			// Disable the mouse while the menu is open.
			engine->ClientCmd_Unrestricted("cl_mouseenable 0");
			mouse_enabled = false;
		}
	} else {
		if (!mouse_enabled) {
			// Re-enable the mouse while the menu is closed.
			engine->ClientCmd_Unrestricted("cl_mouseenable 1");
			mouse_enabled = true;
		}
	}
	
	// Enable the in-built cursor when the GUI is active.
	ImGui::GetIO().MouseDrawCursor = is_renderer_active;
	
	// Call the original while the GUI is inactive.
	if (!is_renderer_active)
		return oEndScene(thisptr);

	ImGui_ImplDX9_NewFrame();

	// Place all item settings under a collapsing header.
	if (ImGui::CollapsingHeader("Items")) {
		// Loop through the game item list.
		for (const auto& item: ItemDefinitionIndex) {
			// Get the configuration for the current item.
			EconomyItem_t& weapon = config.GetWeaponConfiguration(item.first);

			// Ensure that our settings will be used.
			if (!weapon.is_valid)
				weapon.is_valid = true;

			// Create a new node in the tree for this item.
			if (ImGui::TreeNode(item.second.display_name)) {
				// Add input forms to edit values for this item.
				ImGui::InputInt(std::string("Item Index##").append(item.second.entity_name).c_str(), &weapon.item_definition_index);
				ImGui::InputInt(std::string("Paint Kit##").append(item.second.entity_name).c_str(), &weapon.fallback_paint_kit);
				ImGui::InputInt(std::string("Seed##").append(item.second.entity_name).c_str(), &weapon.fallback_seed);
				ImGui::InputInt(std::string("Quality##").append(item.second.entity_name).c_str(), &weapon.entity_quality);
				ImGui::InputInt(std::string("StatTrak##").append(item.second.entity_name).c_str(), &weapon.fallback_stattrak);
				ImGui::InputFloat(std::string("Wear##").append(item.second.entity_name).c_str(), &weapon.fallback_wear);
				ImGui::InputText(std::string("Name Tag##").append(item.second.entity_name).c_str(), weapon.custom_name, 32);

				// Add a placeholer 'Apply' button that calls 'cl_fullupdate'.
				if (ImGui::Button("Apply", ImVec2(ImGui::GetContentRegionAvail().x, 20)))
					engine->ClientCmd_Unrestricted("cl_fullupdate");

				ImGui::TreePop();
			}
		}
	}

	if (ImGui::CollapsingHeader("Presets")) {
		static std::vector<std::string> presets = config.GetPresets();

		static char preset_filename[64];
		ImGui::Text("Preset filename");
		ImGui::InputText("##current_filename", preset_filename, 64);
		ImGui::SameLine();
		
		if (ImGui::Button("New")) {
			config.SavePreset(preset_filename);
			presets = config.GetPresets();
		}

		static bool reset_on_load = false;
		ImGui::Checkbox("Reset settings on load.", &reset_on_load);

		if (presets.size() >= 1) {
			for (const std::string& preset: presets) {
				ImGui::AlignFirstTextHeightToWidgets();
				
				ImGui::BulletText(preset.c_str());
				ImGui::SameLine();

				if (ImGui::Button(std::string("Save##").append(preset).c_str()))
					config.SavePreset(preset);

				ImGui::SameLine();

				if (ImGui::Button(std::string("Load##").append(preset).c_str())) {
					config.LoadPreset(preset, reset_on_load);
					engine->ClientCmd_Unrestricted("cl_fullupdate");
				}

				ImGui::SameLine();
				
				if (ImGui::Button(std::string("Delete##").append(preset).c_str()) && config.RemovePreset(preset.c_str()))
					presets = config.GetPresets();

				ImGui::Spacing();
			}
		} else {
			ImGui::TextWrapped("No presets found. You can create a preset by entering a name above and pressing the 'New' button.");
		}

		if (ImGui::Button("Refresh"))
			presets = config.GetPresets();
	}

	ImGui::Render();

	// Finally, call the original function.
	return oEndScene(thisptr);
}