#include <list>
#include <vector>
#include <string.h>
#include <pthread.h>
#include <thread>
#include <cstring>
#include <jni.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <dlfcn.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include "Includes/Logger.h"
#include "Includes/obfuscate.h"
#include "Includes/Utils.h"
#include "KittyMemory/MemoryPatch.h"
#include "includes/Dobby/dobby.h"
#include "Color.h"
#include "Includes/Macros.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_android.h"
#define targetLibName OBFUSCATE("libil2cpp.so")
#include "ByNameModding/BNM.hpp"
using namespace BNM;
int glHeight, glWidth;
bool setup;
uintptr_t address;

// Function to get offset information
uintptr_t getOffsetInfo(const char* className, const char* methodName) {
    // Check if BNM is initialized
    if (!BNM::IsLoaded()) {
        return 0;
    }
    
    auto targetClass = BNM::Class(className);
    if (!targetClass.Valid()) {
        return 0;
    }
    
    auto method = targetClass.GetMethod(methodName);
    if (!method.Valid()) {
        return 0;
    }
    
    return (uintptr_t)method.GetAddress() - address;
}

// Function to read value at offset
template<typename T>
T readOffsetValue(uintptr_t offset) {
    if (address == 0 || offset == 0) {
        return T{};
    }
    
    uintptr_t targetAddress = address + offset;
    return *(T*)targetAddress;
}

// Function to read string at offset
std::string readStringValue(uintptr_t offset) {
    if (address == 0 || offset == 0) {
        return "";
    }
    
    uintptr_t targetAddress = address + offset;
    char* strPtr = *(char**)targetAddress;
    
    if (strPtr == nullptr) {
        return "[null]";
    }
    
    // Safety check to avoid reading invalid memory
    try {
        // Check if string is valid by reading first few bytes
        if (strPtr < (char*)0x1000) {
            return "[invalid ptr]";
        }
        
        // Read string with length limit for safety
        std::string result;
        for (int i = 0; i < 512; i++) { // Max 512 chars
            char c = strPtr[i];
            if (c == '\0') break;
            
            // Handle UTF-8 sequences (basic support)
            if ((unsigned char)c >= 0x80) {
                // Multi-byte UTF-8 character
                result += c;
                // Continue reading UTF-8 sequence
                if (i + 1 < 512 && strPtr[i + 1] != '\0') {
                    result += strPtr[++i];
                    if ((unsigned char)c >= 0xE0 && i + 1 < 512 && strPtr[i + 1] != '\0') {
                        result += strPtr[++i]; // 3-byte UTF-8
                    }
                    if ((unsigned char)c >= 0xF0 && i + 1 < 512 && strPtr[i + 1] != '\0') {
                        result += strPtr[++i]; // 4-byte UTF-8
                    }
                }
            } else {
                result += c;
            }
        }
        
        return result.empty() ? "[empty]" : result;
    } catch (...) {
        return "[read error]";
    }
}

#define HOOKAF(ret, func, ...) \
    ret (*orig##func)(__VA_ARGS__); \
    ret my##func(__VA_ARGS__)

HOOKAF(void, Input, void *thiz, void *ex_ab, void *ex_ac) {
    origInput(thiz, ex_ab, ex_ac);
    ImGui_ImplAndroid_HandleInputEvent((AInputEvent *)thiz);
    return;
}

void SetupImgui() {
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    io.DisplaySize = ImVec2((float)glWidth, (float)glHeight);

    // Setup Dear ImGui style
    // Setup Platform/Renderer backends
    ImGui_ImplOpenGL3_Init("#version 100");

    // We load the default font with increased size to improve readability on many devices with "high" DPI.
    ImFontConfig font_cfg;
    font_cfg.SizePixels = 22.0f;
    
    // Add Cyrillic glyph ranges for Russian text support
    static const ImWchar cyrillic_ranges[] = {
        0x0020, 0x00FF, // Basic Latin + Latin Supplement
        0x0400, 0x052F, // Cyrillic + Cyrillic Supplement
        0x2DE0, 0x2DFF, // Cyrillic Extended-A
        0xA640, 0xA69F, // Cyrillic Extended-B
        0,
    };
    
    font_cfg.GlyphRanges = cyrillic_ranges;
    io.Fonts->AddFontDefault(&font_cfg);

    // Custom dark theme based on provided design
    ImGuiStyle& style = ImGui::GetStyle();
    
    // Window styling
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.WindowPadding = ImVec2(10, 10);
    style.FramePadding = ImVec2(8, 4);
    style.ItemSpacing = ImVec2(8, 6);
    
    // Dark theme colors matching the screenshot
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.15f, 0.15f, 0.15f, 0.95f);  // Dark gray background
    colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.12f, 0.12f, 0.95f);
    colors[ImGuiCol_Border] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);   // Input fields
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);      // Buttons
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);        // Light gray text
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

    // Arbitrary scale-up
    ImGui::GetStyle().ScaleAllSizes(3.0f);
}


EGLBoolean (*old_eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);
EGLBoolean hook_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    eglQuerySurface(dpy, surface, EGL_WIDTH, &glWidth);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &glHeight);

    if (!setup) {
        SetupImgui();
        setup = true;
    }

    ImGuiIO &io = ImGui::GetIO();


    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
    
    // Main menu button (always visible after injection)
    static bool showModWindow = false;
    
    ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
    ImGui::Begin("Menu", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
    
    if (ImGui::Button("Open")) {
        showModWindow = !showModWindow;
    }
    
    ImGui::End();
    
    // Mod window (opens on button press)
    if (showModWindow) {
        ImGui::SetNextWindowPos(ImVec2(200, 100), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
        ImGui::Begin("Offset Info Window", &showModWindow);
        
        ImGui::Text("Unity IL2CPP Offset Information");
        ImGui::Text("Тест кириллицы: Привет Мир!");
        ImGui::Separator();
        
        // Input fields for class and method names
        static char className[256] = "";
        static char methodName[256] = "";
        
        ImGui::InputText("Class Name", className, sizeof(className));
        ImGui::InputText("Method Name", methodName, sizeof(methodName));
        
        static uintptr_t currentOffset = 0;
        static bool offsetFound = false;
        
        if (ImGui::Button("Get Offset")) {
            if (strlen(className) > 0 && strlen(methodName) > 0) {
                currentOffset = getOffsetInfo(className, methodName);
                offsetFound = (currentOffset != 0);
            }
        }
        
        ImGui::Separator();
        
        if (offsetFound) {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "Offset Found!");
            ImGui::Text("Class: %s", className);
            ImGui::Text("Method: %s", methodName);
            ImGui::Text("Offset: 0x%lX", currentOffset);
            
            // Copy to clipboard button
            if (ImGui::Button("Copy Offset")) {
                char offsetStr[32];
                snprintf(offsetStr, sizeof(offsetStr), "0x%lX", currentOffset);
                ImGui::SetClipboardText(offsetStr);
            }
        } else if (strlen(className) > 0 && strlen(methodName) > 0) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Offset not found or invalid class/method");
        }
        
        ImGui::Separator();
        ImGui::Text("Read Value by Offset:");
        
        // Manual offset input
        static char offsetInput[32] = "";
        static int dataType = 0; // 0=int, 1=float, 2=bool, 3=uintptr_t, 4=string
        static bool valueRead = false;
        static union {
            int intVal;
            float floatVal;
            bool boolVal;
            uintptr_t ptrVal;
        } readValue;
        static std::string stringValue;
        
        ImGui::InputText("Offset (hex)", offsetInput, sizeof(offsetInput));
        ImGui::Combo("Data Type", &dataType, "int\0float\0bool\0uintptr_t\0string\0");
        
        if (ImGui::Button("Read Value")) {
            if (strlen(offsetInput) > 0) {
                uintptr_t inputOffset = strtoul(offsetInput, nullptr, 16);
                if (inputOffset > 0) {
                    switch (dataType) {
                        case 0: readValue.intVal = readOffsetValue<int>(inputOffset); break;
                        case 1: readValue.floatVal = readOffsetValue<float>(inputOffset); break;
                        case 2: readValue.boolVal = readOffsetValue<bool>(inputOffset); break;
                        case 3: readValue.ptrVal = readOffsetValue<uintptr_t>(inputOffset); break;
                        case 4: stringValue = readStringValue(inputOffset); break;
                    }
                    valueRead = true;
                }
            }
        }
        
        if (valueRead && strlen(offsetInput) > 0) {
            ImGui::Text("Offset: %s", offsetInput);
            switch (dataType) {
                case 0: ImGui::Text("Value (int): %d", readValue.intVal); break;
                case 1: ImGui::Text("Value (float): %.3f", readValue.floatVal); break;
                case 2: ImGui::Text("Value (bool): %s", readValue.boolVal ? "true" : "false"); break;
                case 3: ImGui::Text("Value (ptr): 0x%lX", readValue.ptrVal); break;
                case 4: ImGui::Text("Value (string): %s", stringValue.c_str()); break;
            }
        }
        
        ImGui::End();
    }
    
    // Rendering
    ImGui::EndFrame();
    ImGui::Render();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());


    return old_eglSwapBuffers(dpy, surface);
}


void *hack_thread(void *) {
    LOGI(OBFUSCATE("pthread created"));
    sleep(25);
    do {
        sleep(1);
    } while (!isLibraryLoaded("libil2cpp.so"));

    // Initialize BNM after il2cpp is loaded
    address = findLibrary("libil2cpp.so");
    BNM::TryForceLoadIl2CppByPath(targetLibName);
    BNM::LoadIl2Cpp();
    
    pthread_exit(nullptr);
    return nullptr;
}

void *imgui_go(void *) {
    auto addr = (uintptr_t)dlsym(RTLD_NEXT, "eglSwapBuffers");
    DobbyHook((void *)addr, (void *)hook_eglSwapBuffers, (void **)&old_eglSwapBuffers);
    pthread_exit(nullptr);
    return nullptr;
}

__attribute__((constructor))
void lib_main() {
    // Create a new thread so it does not block the main thread, means the game would not freeze
    pthread_t ptid;
    pthread_create(&ptid, NULL, imgui_go, NULL);
    pthread_t hacks;
    pthread_create(&hacks, NULL, hack_thread, NULL);
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void * reserved)
{
    JNIEnv *env;
    vm->GetEnv((void **) &env, JNI_VERSION_1_6);
    void *sym_input = DobbySymbolResolver(("/system/lib/libinput.so"), ("_ZN7android13InputConsumer21initializeMotionEventEPNS_11MotionEventEPKNS_12InputMessageE"));
    if (NULL != sym_input) {
        DobbyHook((void *)sym_input, (void *) myInput, (void **)&origInput);
    }
    return JNI_VERSION_1_6;
}
