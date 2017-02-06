#include "gui_d3d.h"
#include "gui_wnd.h"

void GUI_Render()
{
	ImGuiIO& io = ImGui::GetIO();
	io.MouseDrawCursor = false;
	ImGui_ImplDX9_NewFrame();

	bool show = true; // Always show the main (virtual) window
	
	ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiSetCond_Always);
	ImGui::SetNextWindowSize(ImVec2(g_d3d.present_params.BackBufferWidth, g_d3d.present_params.BackBufferHeight), ImGuiSetCond_Always);
	ImGui::Begin("MODSound_GUI", &show, 
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_MenuBar |
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_HorizontalScrollbar);

	if (ImGui::BeginMenuBar())
	{
		//if (ImGui::BeginMenu("File"))
		//{
		//	if (ImGui::MenuItem("Close")) *p_open = false;
		//	ImGui::EndMenu();
		//}
		
		if (ImGui::BeginMenu("File"))
		{
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit"))
		{
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View"))
		{
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Help"))
		{
			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}

	ImGui::End();

#if 0
	//
	// Draw the ImGui test window
	//
	bool show_test_window = true;
	ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiSetCond_FirstUseEver);
	ImGui::ShowTestWindow(&show_test_window);
#endif

	//
	// Render
	//
	g_d3d.device->SetRenderState(D3DRS_ZENABLE, false);
	g_d3d.device->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
	g_d3d.device->SetRenderState(D3DRS_SCISSORTESTENABLE, false);

	g_d3d.device->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3D_CLEARCOLOR, 1.0f, 0);

	if (g_d3d.device->BeginScene() >= 0)
	{
		ImGui::Render();
		g_d3d.device->EndScene();
	}

	g_d3d.device->Present(NULL, NULL, NULL, NULL);
}

LRESULT WINAPI GUI_WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplDX9_WndProcHandler(hWnd, msg, wParam, lParam))
	{
		return true;
	}
	
	switch (msg)
	{
	case WM_SIZE:
		if (g_d3d.device != NULL && wParam != SIZE_MINIMIZED)
		{
			ImGui_ImplDX9_InvalidateDeviceObjects();
			
			//
			// Only updating the viewport is faster but leads to stretching etc.
			/*
			D3DVIEWPORT9 viewport;
			viewport.Height = HIWORD(lParam);
			viewport.Width = LOWORD(lParam) * 2;
			viewport.X = 0;
			viewport.Y = 0;
			viewport.MinZ = 0.0f;
			viewport.MaxZ = 0.0f;
			g_d3d.device->SetViewport(&viewport);
			
			GUI_Render();
			*/
			
			g_d3d.present_params.BackBufferWidth = LOWORD(lParam);
			g_d3d.present_params.BackBufferHeight = HIWORD(lParam);
			
			HRESULT hr = g_d3d.device->Reset(&g_d3d.present_params);
			if (hr == D3DERR_INVALIDCALL)
				IM_ASSERT(0);

			ImGui_ImplDX9_CreateDeviceObjects();

			GUI_Render();
		}
		return 0;
	case WM_SYSCOMMAND:
		// Disable ALT application menu
		if ((wParam & 0xFFF0) == SC_KEYMENU)
			return 0;
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

int GUI_InitWindow(wnd_instance_t* wnd)
{
	//
	// Register Window Class
	//
	WNDCLASSEX* wc = &wnd->wc;
	wc->cbSize = sizeof(WNDCLASSEX);
	wc->style = CS_CLASSDC;
	wc->lpfnWndProc = GUI_WndProc;
	wc->cbClsExtra = 0;
	wc->cbWndExtra = 0;
	wc->hInstance = GetModuleHandle(NULL);
	wc->hIcon = NULL; // TODO: Icon
	wc->hCursor = LoadCursor(NULL, IDC_ARROW);
	wc->hbrBackground = NULL;
	wc->lpszMenuName = NULL;
	wc->lpszClassName = WNDCLASS_NAME;
	wc->hIconSm = NULL; // TODO: Small Icon

	RegisterClassEx(wc);

	//
	// Create Window
	//
	int x = CW_USEDEFAULT;
	int y = CW_USEDEFAULT;

	int size_x = CW_USEDEFAULT;
	int size_y = CW_USEDEFAULT;

	wnd->hWnd = CreateWindow(WNDCLASS_NAME, WND_TITLE, WS_OVERLAPPEDWINDOW, x, y, size_x, size_y, NULL, NULL, wc->hInstance, NULL);

	if (!wnd->hWnd)
	{
		return 1;
	}

	wnd->hasEnteredMessageLoop = false;
	return 0;
}

void GUI_FreeWindow(wnd_instance_t* wnd)
{
	UnregisterClass(WNDCLASS_NAME, wnd->wc.hInstance);
}

void GUI_EnterMessageLoop(wnd_instance_t* wnd)
{
	MSG msg;
	ZeroMemory(&msg, sizeof(msg));

	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			continue;
		}
		
		GUI_Render();
		
		if (!wnd->hasEnteredMessageLoop)
		{
			wnd->hasEnteredMessageLoop = true;
			UpdateWindow(wnd->hWnd);
			ShowWindow(wnd->hWnd, SW_SHOWDEFAULT);
		}
	}
}
