#include "window.h"

#include <windowsx.h>

using namespace cg::utils;

HWND window::hwnd = nullptr;

bool window::pressed_w = false;
bool window::pressed_a = false;
bool window::pressed_s = false;
bool window::pressed_d = false;
bool window::pressed_space = false;
bool window::pressed_ctrl = false;
int window::last_x = -1;
int window::last_y = -1;
bool window::reset_frame = true;

constexpr unsigned char KEY_W = 87;
constexpr unsigned char KEY_A = 65;
constexpr unsigned char KEY_S = 83;
constexpr unsigned char KEY_D = 68;
constexpr unsigned char KEY_SPACE = 0x20;
constexpr unsigned char KEY_CTRL = 0x11;

int cg::utils::window::run(cg::renderer::renderer* renderer, HINSTANCE hinstance, int ncmdshow)
{
	// Initialize the window class.
	WNDCLASSEX window_class = {};
	window_class.cbSize = sizeof(WNDCLASSEX);
	window_class.style = CS_HREDRAW | CS_VREDRAW;
	window_class.lpfnWndProc = window_proc;
	window_class.hInstance = hinstance;
	window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
	window_class.lpszClassName = L"DXSampleClass";
	RegisterClassEx(&window_class);

	// Create the window and store a handle to it.
	RECT window_rect = {0, 0, static_cast<LONG>(renderer->get_width()),
						static_cast<LONG>(renderer->get_height())};
	AdjustWindowRect(&window_rect, WS_OVERLAPPEDWINDOW, FALSE);
	hwnd = CreateWindow(
			window_class.lpszClassName, L"DX12 renderer", WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, window_rect.right - window_rect.left,
			window_rect.bottom - window_rect.top, nullptr, nullptr, hinstance, renderer);

	// Initialize the sample. OnInit is defined in each child-implementation of DXSample.
	renderer->init();
	ShowWindow(hwnd, ncmdshow);
	ShowCursor(false);
	// Main sample loop.
	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	renderer->destroy();
	// Return this part of the WM_QUIT message to Windows.
	return static_cast<int>(msg.wParam);
}

LRESULT cg::utils::window::window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	cg::renderer::renderer* renderer = reinterpret_cast<cg::renderer::renderer*>(
			GetWindowLongPtr(hwnd, GWLP_USERDATA));

	switch (message)
	{
		case WM_CREATE: {
			// Save the Renderer* passed in to CreateWindow.
			LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lparam);
			SetWindowLongPtr(
					hwnd, GWLP_USERDATA,
					reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
			return 0;
		}

		case WM_PAINT:
			if (renderer) {
				if (window::pressed_w)
					renderer->move_forward(10.f);
				if (window::pressed_s)
					renderer->move_backward(10.f);
				if (window::pressed_a)
					renderer->move_left(10.f);
				if (window::pressed_d)
					renderer->move_right(10.f);
				if (window::pressed_ctrl)
					renderer->move_down(10.f);
				if (window::pressed_space)
					renderer->move_up(10.f);
				
				renderer->update();
				renderer->render();
			}
			return 0;

		case WM_KEYDOWN:
			switch (static_cast<UINT8>(wparam)) {
				case KEY_W:
					window::pressed_w = true;
					break;
				case KEY_A:
					window::pressed_a = true;
					break;
				case KEY_S:
					window::pressed_s = true;
					break;
				case KEY_D:
					window::pressed_d = true;
					break;
				case KEY_SPACE:
					window::pressed_space = true;
					break;
				case KEY_CTRL:
					window::pressed_ctrl = true;
					break;
			}
			return 0;

		case WM_KEYUP: 
			switch (static_cast<UINT8>(wparam)) {
				case KEY_W:
					window::pressed_w = false;
					break;
				case KEY_A:
					window::pressed_a = false;
					break;
				case KEY_S:
					window::pressed_s = false;
					break;
				case KEY_D:
					window::pressed_d = false;
					break;
				case KEY_SPACE:
					window::pressed_space = false;
					break;
				case KEY_CTRL:
					window::pressed_ctrl = false;
					break;
			}
			return 0;

		case WM_MOUSEMOVE:
			if (renderer) {
				short x_pos = GET_X_LPARAM(lparam);
				short y_pos = GET_Y_LPARAM(lparam);
				float sensitivity = 500;

				if (last_x >= 0 && !reset_frame) {
					short x_delta = x_pos - last_x;
					short y_delta = y_pos - last_y;

					renderer->move_yaw(sensitivity * static_cast<float>(x_delta) / renderer->get_width());
					renderer->move_pitch(-sensitivity * static_cast<float>(y_delta) / renderer->get_height());

					SetCursorPos(renderer->get_width() / 2, renderer->get_height() / 2);
				}

				reset_frame = !reset_frame;
				last_x = x_pos;
				last_y = y_pos;
			}
			return 0;

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
	}

	// Handle any messages the switch statement didn't.
	return DefWindowProc(hwnd, message, wparam, lparam);
}
