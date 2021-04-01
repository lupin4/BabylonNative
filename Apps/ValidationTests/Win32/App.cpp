// App.cpp : Defines the entry point for the application.
//

#include "App.h"
// NOMINMAX to prevent compilation errors with bgfx
#define NOMINMAX
#include <Windows.h>
#undef NOMINMAX
#include <Windowsx.h>
#include <Shlwapi.h>
#include <filesystem>
#include <algorithm>

#include <Shared/TestUtils.h>

#include <Babylon/AppRuntime.h>
#include <Babylon/Graphics.h>
#include <Babylon/GraphicsPlatform.h>
#include <Babylon/ScriptLoader.h>
#include <Babylon/Plugins/NativeEngine.h>
#include <Babylon/Polyfills/Console.h>
#include <Babylon/Polyfills/Window.h>
#include <Babylon/Polyfills/XMLHttpRequest.h>
#include <iostream>

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
std::unique_ptr<Babylon::Graphics> graphics{};
std::unique_ptr<Babylon::AppRuntime> runtime{};

// 600, 400 mandatory size for CI tests
static const int TEST_WIDTH = 600;
static const int TEST_HEIGHT = 400;

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

namespace
{
    std::filesystem::path GetModulePath()
    {
        char buffer[1024];
        ::GetModuleFileNameA(nullptr, buffer, ARRAYSIZE(buffer));
        return std::filesystem::path{ buffer };
    }

    std::string GetUrlFromPath(const std::filesystem::path& path)
    {
        char url[1024];
        DWORD length = ARRAYSIZE(url);
        HRESULT hr = UrlCreateFromPathA(path.u8string().data(), url, &length, 0);
        if (FAILED(hr))
        {
            throw std::exception("Failed to create url from path", hr);
        }

        return { url };
    }

    std::vector<std::string> GetCommandLineArguments()
    {
        int argc;
        auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);

        std::vector<std::string> arguments{};
        arguments.reserve(argc);

        for (int idx = 1; idx < argc; idx++)
        {
            std::wstring hstr{ argv[idx] };
            int bytesRequired = ::WideCharToMultiByte(CP_UTF8, 0, &hstr[0], static_cast<int>(hstr.size()), nullptr, 0, nullptr, nullptr);
            arguments.push_back(std::string(bytesRequired, 0));
            ::WideCharToMultiByte(CP_UTF8, 0, hstr.data(), static_cast<int>(hstr.size()), arguments.back().data(), bytesRequired, nullptr, nullptr);
        }

        LocalFree(argv);

        return arguments;
    }

    void Uninitialize()
    {
        if (graphics)
        {
            graphics->FinishRenderingCurrentFrame();
        }

        runtime.reset();
        graphics.reset();
    }

    void Initialize(HWND hWnd)
    {
        Babylon::GraphicsConfiguration graphicsConfig{};
        graphicsConfig.WindowPtr = hWnd;
        graphicsConfig.Width = static_cast<size_t>(TEST_WIDTH);
        graphicsConfig.Height = static_cast<size_t>(TEST_HEIGHT);

        graphics = Babylon::Graphics::CreateGraphics(graphicsConfig);
        graphics->SetDiagnosticOutput([](const char* outputString) { printf("%s", outputString); fflush(stdout); });
        graphics->StartRenderingCurrentFrame();

        runtime = std::make_unique<Babylon::AppRuntime>();

        // Initialize console plugin.
        runtime->Dispatch([hWnd](Napi::Env env) {
            graphics->AddToJavaScript(env);

            Babylon::Polyfills::Console::Initialize(env, [](const char* message, auto) {
                OutputDebugStringA(message);

                printf("%s", message);
                fflush(stdout);
            });

            Babylon::Polyfills::Window::Initialize(env);

            Babylon::Polyfills::XMLHttpRequest::Initialize(env);

            Babylon::Plugins::NativeEngine::Initialize(env);

            Babylon::TestUtils::CreateInstance(env, hWnd);
        });

        // Scripts are copied to the parent of the executable due to CMake issues.
        // See the CMakeLists.txt comments for more details.
        std::string scriptsRootUrl = GetUrlFromPath(GetModulePath().parent_path().parent_path() / "Scripts");

        Babylon::ScriptLoader loader{ *runtime };
        loader.LoadScript(scriptsRootUrl + "/babylon.max.js");
        loader.LoadScript(scriptsRootUrl + "/babylon.glTF2FileLoader.js");
        loader.LoadScript(scriptsRootUrl + "/babylonjs.materials.js");
        loader.LoadScript(scriptsRootUrl + "/babylon.gui.js");
        loader.LoadScript(scriptsRootUrl + "/validation_native.js");
    }
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_VALIDATIONTESTSWIN32, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_VALIDATIONTESTSWIN32));

    MSG msg{};

    // Main message loop:
    while (msg.message != WM_QUIT)
    {
        if (graphics)
        {
            graphics->FinishRenderingCurrentFrame();
            graphics->StartRenderingCurrentFrame();
        }

        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) && msg.message != WM_QUIT)
        {
            if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }

    return (int)msg.wParam;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_VALIDATIONTESTSWIN32));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = 0;
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // Store instance handle in our global variable
    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, TEST_WIDTH, TEST_HEIGHT, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    Initialize(hWnd);

    return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_SIZE:
        {
            if (graphics)
            {
                size_t width = static_cast<size_t>(LOWORD(lParam));
                size_t height = static_cast<size_t>(HIWORD(lParam));
                graphics->UpdateSize(width, height);
            }
            break;
        }
        case WM_DESTROY:
        {
            Uninitialize();
            PostQuitMessage(errorCode);
            break;
        }
        default:
        {
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    return 0;
}
