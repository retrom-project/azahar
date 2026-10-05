// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <list>
#include <numeric>
#include <vector>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef ENABLE_OPENGL
#include "glad/glad.h"
#include "video_core/renderer_opengl/gl_vars.h"
#endif
#include "libretro.h"

#include "audio_core/libretro_input.h"
#include "audio_core/libretro_sink.h"
#include "video_core/gpu.h"
#ifdef ENABLE_OPENGL
#include "video_core/renderer_opengl/renderer_opengl.h"
#endif
#ifdef ENABLE_VULKAN
#include "citra_libretro/libretro_vk.h"
#endif
#include "video_core/renderer_software/renderer_software.h"
#include "video_core/video_core.h"

#include "citra_libretro/citra_libretro.h"
#include "citra_libretro/core_settings.h"
#include "citra_libretro/environment.h"
#include "citra_libretro/input/input_factory.h"

#include "common/arch.h"
#if CITRA_ARCH(x86_64)
#include "common/x64/cpu_detect.h"
#endif
#include "common/logging/backend.h"
#include "common/logging/filter.h"
#include "common/settings.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/frontend/applets/default_applets.h"
#include "core/frontend/image_interface.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/memory.h"
#include "core/hle/kernel/process.h"
#include "core/loader/loader.h"
#include "core/memory.h"

#ifdef HAVE_LIBRETRO_VFS
#include <streams/file_stream_transforms.h>
#endif

class CitraLibRetro {
public:
    CitraLibRetro() : log_filter(Common::Log::Level::Debug) {}

    Common::Log::Filter log_filter;
    std::unique_ptr<EmuWindow_LibRetro> emu_window;
    bool game_loaded = false;
    struct retro_hw_render_callback hw_render{};
};

CitraLibRetro* emu_instance;

void retro_init() {
    emu_instance = new CitraLibRetro();
    Common::Log::LibRetroStart(LibRetro::GetLoggingBackend());
    Common::Log::SetGlobalFilter(emu_instance->log_filter);

    LOG_DEBUG(Frontend, "Initializing core...");

    // Set up LLE cores
    for (const auto& service_module : Service::service_module_map) {
        Settings::values.lle_modules.emplace(service_module.name, false);
    }

    // Setup default, stub handlers for HLE applets
    Frontend::RegisterDefaultApplets(Core::System::GetInstance());

    // Register generic image interface
    Core::System::GetInstance().RegisterImageInterface(
        std::make_shared<Frontend::ImageInterface>());

    LibRetro::Input::Init();
}

void retro_deinit() {
    LOG_DEBUG(Frontend, "Shutting down core...");
    if (Core::System::GetInstance().IsPoweredOn()) {
        Core::System::GetInstance().Shutdown();
    }

    LibRetro::Input::Shutdown();

    delete emu_instance;

    Common::Log::Stop();
}

unsigned retro_api_version() {
    return RETRO_API_VERSION;
}

void LibRetro::OnConfigureEnvironment() {

#ifdef HAVE_LIBRETRO_VFS
    struct retro_vfs_interface_info vfs_iface_info{1, nullptr};
    LibRetro::SetVFSCallback(&vfs_iface_info);
#endif

    LibRetro::RegisterCoreOptions();

    static const struct retro_controller_description controllers[] = {
        {"Nintendo 3DS", RETRO_DEVICE_JOYPAD},
    };

    static const struct retro_controller_info ports[] = {
        {controllers, 1},
        {nullptr, 0},
    };

    LibRetro::SetControllerInfo(ports);
}

uintptr_t LibRetro::GetFramebuffer() {
    return emu_instance->hw_render.get_current_framebuffer();
}

/**
 * Updates Citra's settings with Libretro's.
 */
static void UpdateSettings() {
    LibRetro::ParseCoreOptions();

    struct retro_input_descriptor desc[] = {
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left"},
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Up"},
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Down"},
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right"},
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "X"},
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Y"},
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B"},
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A"},
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "L"},
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "ZL"},
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "R"},
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "ZR"},
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start"},
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select"},
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3, "Home/Swap screens"},
        {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, "Touch Screen Touch"},
        {0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X,
         "Circle Pad X"},
        {0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y,
         "Circle Pad Y"},
        {0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X,
         "C-Stick / Pointer X"},
        {0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y,
         "C-Stick / Pointer Y"},
        {0, 0},
    };

    LibRetro::SetInputDescriptors(desc);

    Settings::values.current_input_profile.touch_device = "engine:emu_window";

    // Hardcode buttons to bind to libretro - it is entirely redundant to have
    //  two methods of rebinding controls.
    // Citra: A = RETRO_DEVICE_ID_JOYPAD_A (8)
    Settings::values.current_input_profile.buttons[Settings::NativeButton::Values::A] =
        "button:8,joystick:0,engine:libretro";
    // Citra: B = RETRO_DEVICE_ID_JOYPAD_B (0)
    Settings::values.current_input_profile.buttons[Settings::NativeButton::Values::B] =
        "button:0,joystick:0,engine:libretro";
    // Citra: X = RETRO_DEVICE_ID_JOYPAD_X (9)
    Settings::values.current_input_profile.buttons[Settings::NativeButton::Values::X] =
        "button:9,joystick:0,engine:libretro";
    // Citra: Y = RETRO_DEVICE_ID_JOYPAD_Y (1)
    Settings::values.current_input_profile.buttons[Settings::NativeButton::Values::Y] =
        "button:1,joystick:0,engine:libretro";
    // Citra: UP = RETRO_DEVICE_ID_JOYPAD_UP (4)
    Settings::values.current_input_profile.buttons[Settings::NativeButton::Values::Up] =
        "button:4,joystick:0,engine:libretro";
    // Citra: DOWN = RETRO_DEVICE_ID_JOYPAD_DOWN (5)
    Settings::values.current_input_profile.buttons[Settings::NativeButton::Values::Down] =
        "button:5,joystick:0,engine:libretro";
    // Citra: LEFT = RETRO_DEVICE_ID_JOYPAD_LEFT (6)
    Settings::values.current_input_profile.buttons[Settings::NativeButton::Values::Left] =
        "button:6,joystick:0,engine:libretro";
    // Citra: RIGHT = RETRO_DEVICE_ID_JOYPAD_RIGHT (7)
    Settings::values.current_input_profile.buttons[Settings::NativeButton::Values::Right] =
        "button:7,joystick:0,engine:libretro";
    // Citra: L = RETRO_DEVICE_ID_JOYPAD_L (10)
    Settings::values.current_input_profile.buttons[Settings::NativeButton::Values::L] =
        "button:10,joystick:0,engine:libretro";
    // Citra: R = RETRO_DEVICE_ID_JOYPAD_R (11)
    Settings::values.current_input_profile.buttons[Settings::NativeButton::Values::R] =
        "button:11,joystick:0,engine:libretro";
    // Citra: START = RETRO_DEVICE_ID_JOYPAD_START (3)
    Settings::values.current_input_profile.buttons[Settings::NativeButton::Values::Start] =
        "button:3,joystick:0,engine:libretro";
    // Citra: SELECT = RETRO_DEVICE_ID_JOYPAD_SELECT (2)
    Settings::values.current_input_profile.buttons[Settings::NativeButton::Values::Select] =
        "button:2,joystick:0,engine:libretro";
    // Citra: ZL = RETRO_DEVICE_ID_JOYPAD_L2 (12)
    Settings::values.current_input_profile.buttons[Settings::NativeButton::Values::ZL] =
        "button:12,joystick:0,engine:libretro";
    // Citra: ZR = RETRO_DEVICE_ID_JOYPAD_R2 (13)
    Settings::values.current_input_profile.buttons[Settings::NativeButton::Values::ZR] =
        "button:13,joystick:0,engine:libretro";
    // Citra: HOME = RETRO_DEVICE_ID_JOYPAD_L3 (as per above bindings) (14)
    Settings::values.current_input_profile.buttons[Settings::NativeButton::Values::Home] =
        "button:14,joystick:0,engine:libretro";

    // Circle Pad
    Settings::values.current_input_profile.analogs[0] = "axis:0,joystick:0,engine:libretro";
    // C-Stick
    if (LibRetro::settings.analog_function != LibRetro::CStickFunction::Touchscreen) {
        Settings::values.current_input_profile.analogs[1] = "axis:1,joystick:0,engine:libretro";
    } else {
        Settings::values.current_input_profile.analogs[1] = "";
    }

    if (!emu_instance->emu_window) {
        emu_instance->emu_window = std::make_unique<EmuWindow_LibRetro>();
    }

    // Update the framebuffer sizing.
    emu_instance->emu_window->UpdateLayout();

    Core::System::GetInstance().ApplySettings();
}

/**
 * libretro callback; Called every game tick.
 */
void retro_run() {
    if (!emu_instance || !emu_instance->game_loaded) {
        return;
    }

    // Check to see if we actually have any config updates to process.
    if (LibRetro::HasUpdatedConfig()) {
        LibRetro::ParseCoreOptions();
        Core::System::GetInstance().ApplySettings();
        emu_instance->emu_window->UpdateLayout();
    }

    // Poll microphone input from the frontend and buffer it for the emulator
    // This must be done from the main thread as LibRetro's mic interface is not thread-safe
    if (auto* mic_input = AudioCore::GetLibRetroInput()) {
        mic_input->PollMicrophone();
    }

    // Check if the screen swap button is pressed
    static bool screen_swap_btn_state = false;
    static bool screen_swap_toggled = false;
    bool screen_swap_btn =
        !!LibRetro::CheckInput(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3);
    if (screen_swap_btn != screen_swap_btn_state) {
        if (LibRetro::settings.toggle_swap_screen) {
            if (!screen_swap_btn_state)
                screen_swap_toggled = !screen_swap_toggled;

            if (screen_swap_toggled)
                Settings::values.swap_screen =
                    LibRetro::FetchVariable("citra_swap_screen", "Top") != "Bottom";
            else
                Settings::values.swap_screen =
                    LibRetro::FetchVariable("citra_swap_screen", "Top") == "Bottom";
        } else {
            if (screen_swap_btn)
                Settings::values.swap_screen =
                    LibRetro::FetchVariable("citra_swap_screen", "Top") != "Bottom";
            else
                Settings::values.swap_screen =
                    LibRetro::FetchVariable("citra_swap_screen", "Top") == "Bottom";
        }

        Core::System::GetInstance().ApplySettings();

        // Update the framebuffer sizing.
        emu_instance->emu_window->UpdateLayout();

        screen_swap_btn_state = screen_swap_btn;
    }

#ifdef ENABLE_OPENGL
    if (Settings::values.graphics_api.GetValue() == Settings::GraphicsAPI::OpenGL) {
        // We can't assume that the frontend has been nice and preserved all OpenGL settings. Reset.
        auto last_state = OpenGL::OpenGLState::GetCurState();
        ResetGLState();
        last_state.Apply();
    }
#endif

    while (!emu_instance->emu_window->HasSubmittedFrame()) {
        auto result = Core::System::GetInstance().RunLoop();

        if (result != Core::System::ResultStatus::Success) {
            std::string errorContent = Core::System::GetInstance().GetStatusDetails();
            std::string msg;

            switch (result) {
            case Core::System::ResultStatus::ErrorSystemFiles:
                msg = "Azahar was unable to locate a 3DS system archive: " + errorContent;
                break;
            default:
                msg = "Fatal Error encountered (" + std::to_string(static_cast<int>(result)) +
                      "): " + errorContent;
                break;
            }

            LibRetro::DisplayMessage(msg.c_str());
        }
    }
}

static void setup_memory_maps() {
    auto process = Core::System::GetInstance().Kernel().GetCurrentProcess();
    if (!process)
        return;

    std::vector<retro_memory_descriptor> descs;

    for (const auto& [addr, vma] : process->vm_manager.vma_map) {
        if (vma.type != Kernel::VMAType::BackingMemory)
            continue;
        if (vma.size == 0 || !vma.backing_memory)
            continue;

        // Only expose the well-known user-accessible memory regions
        uint64_t flags = 0;
        if (vma.base >= Memory::HEAP_VADDR && vma.base < Memory::HEAP_VADDR_END) {
            flags = RETRO_MEMDESC_SYSTEM_RAM;
        } else if (vma.base >= Memory::LINEAR_HEAP_VADDR &&
                   vma.base < Memory::LINEAR_HEAP_VADDR_END) {
            flags = RETRO_MEMDESC_SYSTEM_RAM;
        } else if (vma.base >= Memory::NEW_LINEAR_HEAP_VADDR &&
                   vma.base < Memory::NEW_LINEAR_HEAP_VADDR_END) {
            flags = RETRO_MEMDESC_SYSTEM_RAM;
        } else if (vma.base >= Memory::VRAM_VADDR && vma.base < Memory::VRAM_VADDR_END) {
            flags = RETRO_MEMDESC_VIDEO_RAM;
        } else {
            continue;
        }

        retro_memory_descriptor desc = {};
        desc.flags = flags;
        desc.ptr = const_cast<u8*>(vma.backing_memory.GetPtr());
        desc.start = vma.base;
        desc.len = vma.size;

        // select=0 requires power-of-2 len AND start aligned to len.
        // When that doesn't hold, compute a select mask instead.
        bool need_select = (vma.size & (vma.size - 1)) != 0;
        if (!need_select && (vma.base & (vma.size - 1)) != 0)
            need_select = true;

        if (need_select) {
            uint64_t np2 = 1;
            while (np2 < vma.size)
                np2 <<= 1;
            if (vma.base & (np2 - 1)) {
                LOG_WARNING(Frontend, "VMA at 0x{:08X} size 0x{:X} not aligned, skipping", vma.base,
                            vma.size);
                continue;
            }
            desc.select = ~(np2 - 1);
        }

        descs.push_back(desc);
    }

    if (!descs.empty()) {
        retro_memory_map map = {descs.data(), static_cast<unsigned>(descs.size())};
        LibRetro::SetMemoryMaps(&map);
    }
}

#ifdef __EMSCRIPTEN__
extern "C" void retrom_content_load_set(int);
#else
static void retrom_content_load_set(int) {}
#endif

static bool do_load_game() {
    const Core::System::ResultStatus load_result{
        Core::System::GetInstance().Load(*emu_instance->emu_window, LibRetro::settings.file_path)};

    if (load_result != Core::System::ResultStatus::Success) {
        retrom_content_load_set(load_result == Core::System::ResultStatus::ErrorLoader_ErrorEncrypted ? -2 : -1);
    }
    switch (load_result) {
    case Core::System::ResultStatus::Success:
        break; // Expected case
    case Core::System::ResultStatus::ErrorGetLoader:
        LibRetro::DisplayMessage("Failed to obtain loader for specified ROM!");
        return false;
    case Core::System::ResultStatus::ErrorLoader:
        LibRetro::DisplayMessage("Failed to load ROM!");
        return false;
    case Core::System::ResultStatus::ErrorLoader_ErrorEncrypted:
        LibRetro::DisplayMessage("The game that you are trying to load must be decrypted before "
                                 "being used with Azahar.");
        return false;
    case Core::System::ResultStatus::ErrorLoader_ErrorInvalidFormat:
        LibRetro::DisplayMessage("Error while loading ROM: The ROM format is not supported.");
        return false;
    case Core::System::ResultStatus::ErrorLoader_ErrorGbaTitle:
        LibRetro::DisplayMessage(
            "Error loading the specified application as it is GBA Virtual Console");
        return false;
    case Core::System::ResultStatus::ErrorNotInitialized:
        LibRetro::DisplayMessage("CPUCore not initialized");
        return false;
    case Core::System::ResultStatus::ErrorSystemMode:
        LibRetro::DisplayMessage("Failed to determine system mode!");
        return false;
    default:
        LibRetro::DisplayMessage(
            ("Unknown error: " + std::to_string(static_cast<int>(load_result))).c_str());
        return false;
    }

    u64 program_id{};
    Core::System::GetInstance().GetAppLoader().ReadProgramId(program_id);
    Core::System::GetInstance().GPU().ApplyPerProgramSettings(program_id);

    if (Settings::values.use_disk_shader_cache) {
        Core::System::GetInstance().GPU().Renderer().Rasterizer()->LoadDefaultDiskResources(
            false, nullptr);
    }

    setup_memory_maps();
    retrom_content_load_set(1);

    return true;
}

#ifdef ENABLE_OPENGL
static void* load_opengl_func(const char* name) {
    return (void*)emu_instance->hw_render.get_proc_address(name);
}
#endif

static void context_reset() {
    LOG_DEBUG(Frontend, "context_reset");

    switch (Settings::values.graphics_api.GetValue()) {
#ifdef ENABLE_OPENGL
    case Settings::GraphicsAPI::OpenGL:
#if defined(USING_GLES)
        Settings::values.use_gles = true;
        // Set the global GLES flag immediately to ensure any shader compilation
        // that happens before the Driver is created uses the correct version
        OpenGL::GLES = true;
#else
        Settings::values.use_gles = false;
        OpenGL::GLES = false;
#endif
        // Check to see if the frontend provides us with OpenGL symbols
        if (emu_instance->hw_render.get_proc_address != nullptr) {
            bool loaded = Settings::values.use_gles
                              ? gladLoadGLES2Loader((GLADloadproc)load_opengl_func)
                              : gladLoadGLLoader((GLADloadproc)load_opengl_func);

            if (!loaded) {
                LOG_CRITICAL(Frontend, "Glad failed to load (frontend-provided symbols)!");
                return;
            }
#if defined(__EMSCRIPTEN__)
            GLAD_GL_ES_VERSION_2_0 = 1;
            GLAD_GL_ES_VERSION_3_0 = 1;
            {
                glad_glGenSamplers =
                    (PFNGLGENSAMPLERSPROC)load_opengl_func("glGenSamplers");
                glad_glDeleteSamplers =
                    (PFNGLDELETESAMPLERSPROC)load_opengl_func("glDeleteSamplers");
                glad_glSamplerParameteri =
                    (PFNGLSAMPLERPARAMETERIPROC)load_opengl_func("glSamplerParameteri");
                glad_glSamplerParameterf =
                    (PFNGLSAMPLERPARAMETERFPROC)load_opengl_func("glSamplerParameterf");
                glad_glSamplerParameterfv =
                    (PFNGLSAMPLERPARAMETERFVPROC)load_opengl_func("glSamplerParameterfv");
                glad_glBindSampler =
                    (PFNGLBINDSAMPLERPROC)load_opengl_func("glBindSampler");
                glad_glTexStorage2D =
                    (PFNGLTEXSTORAGE2DPROC)load_opengl_func("glTexStorage2D");
                glad_glGenVertexArrays =
                    (PFNGLGENVERTEXARRAYSPROC)load_opengl_func("glGenVertexArrays");
                glad_glBindVertexArray =
                    (PFNGLBINDVERTEXARRAYPROC)load_opengl_func("glBindVertexArray");
                glad_glDeleteVertexArrays =
                    (PFNGLDELETEVERTEXARRAYSPROC)load_opengl_func("glDeleteVertexArrays");
                glad_glDrawBuffers =
                    (PFNGLDRAWBUFFERSPROC)load_opengl_func("glDrawBuffers");
                glad_glUniformBlockBinding =
                    (PFNGLUNIFORMBLOCKBINDINGPROC)load_opengl_func("glUniformBlockBinding");
                glad_glGetUniformBlockIndex =
                    (PFNGLGETUNIFORMBLOCKINDEXPROC)load_opengl_func(
                        "glGetUniformBlockIndex");
                glad_glBindBufferBase =
                    (PFNGLBINDBUFFERBASEPROC)load_opengl_func("glBindBufferBase");
                glad_glBindBufferRange =
                    (PFNGLBINDBUFFERRANGEPROC)load_opengl_func("glBindBufferRange");
                glad_glMapBufferRange =
                    (PFNGLMAPBUFFERRANGEPROC)load_opengl_func("glMapBufferRange");
                glad_glUnmapBuffer =
                    (PFNGLUNMAPBUFFERPROC)load_opengl_func("glUnmapBuffer");
                glad_glFlushMappedBufferRange =
                    (PFNGLFLUSHMAPPEDBUFFERRANGEPROC)load_opengl_func(
                        "glFlushMappedBufferRange");
                glad_glBlitFramebuffer =
                    (PFNGLBLITFRAMEBUFFERPROC)load_opengl_func("glBlitFramebuffer");
                glad_glRenderbufferStorageMultisample =
                    (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC)load_opengl_func(
                        "glRenderbufferStorageMultisample");
                glad_glFramebufferTextureLayer =
                    (PFNGLFRAMEBUFFERTEXTURELAYERPROC)load_opengl_func(
                        "glFramebufferTextureLayer");
                glad_glVertexAttribIPointer =
                    (PFNGLVERTEXATTRIBIPOINTERPROC)load_opengl_func("glVertexAttribIPointer");
                glad_glVertexAttribDivisor =
                    (PFNGLVERTEXATTRIBDIVISORPROC)load_opengl_func("glVertexAttribDivisor");
                glad_glDrawArraysInstanced =
                    (PFNGLDRAWARRAYSINSTANCEDPROC)load_opengl_func("glDrawArraysInstanced");
                glad_glDrawElementsInstanced =
                    (PFNGLDRAWELEMENTSINSTANCEDPROC)load_opengl_func(
                        "glDrawElementsInstanced");
                glad_glFenceSync =
                    (PFNGLFENCESYNCPROC)load_opengl_func("glFenceSync");
                glad_glDeleteSync =
                    (PFNGLDELETESYNCPROC)load_opengl_func("glDeleteSync");
                glad_glClientWaitSync =
                    (PFNGLCLIENTWAITSYNCPROC)load_opengl_func("glClientWaitSync");
                glad_glReadBuffer =
                    (PFNGLREADBUFFERPROC)load_opengl_func("glReadBuffer");
                glad_glGetStringi =
                    (PFNGLGETSTRINGIPROC)load_opengl_func("glGetStringi");
                glad_glUniformMatrix3fv =
                    (PFNGLUNIFORMMATRIX3FVPROC)load_opengl_func("glUniformMatrix3fv");
                glad_glUniform1ui =
                    (PFNGLUNIFORM1UIPROC)load_opengl_func("glUniform1ui");
                glad_glUniform2uiv =
                    (PFNGLUNIFORM2UIVPROC)load_opengl_func("glUniform2uiv");

                glad_glEnable = (PFNGLENABLEPROC)load_opengl_func("glEnable");
                glad_glDisable = (PFNGLDISABLEPROC)load_opengl_func("glDisable");
                glad_glIsEnabled = (PFNGLISENABLEDPROC)load_opengl_func("glIsEnabled");
                glad_glDepthFunc = (PFNGLDEPTHFUNCPROC)load_opengl_func("glDepthFunc");
                glad_glDepthMask = (PFNGLDEPTHMASKPROC)load_opengl_func("glDepthMask");
                glad_glDepthRangef = (PFNGLDEPTHRANGEFPROC)load_opengl_func("glDepthRangef");
                glad_glColorMask = (PFNGLCOLORMASKPROC)load_opengl_func("glColorMask");
                glad_glStencilFunc = (PFNGLSTENCILFUNCPROC)load_opengl_func("glStencilFunc");
                glad_glStencilFuncSeparate =
                    (PFNGLSTENCILFUNCSEPARATEPROC)load_opengl_func("glStencilFuncSeparate");
                glad_glStencilOp = (PFNGLSTENCILOPPROC)load_opengl_func("glStencilOp");
                glad_glStencilOpSeparate =
                    (PFNGLSTENCILOPSEPARATEPROC)load_opengl_func("glStencilOpSeparate");
                glad_glStencilMask = (PFNGLSTENCILMASKPROC)load_opengl_func("glStencilMask");
                glad_glStencilMaskSeparate =
                    (PFNGLSTENCILMASKSEPARATEPROC)load_opengl_func("glStencilMaskSeparate");
                glad_glBlendFunc = (PFNGLBLENDFUNCPROC)load_opengl_func("glBlendFunc");
                glad_glBlendFuncSeparate =
                    (PFNGLBLENDFUNCSEPARATEPROC)load_opengl_func("glBlendFuncSeparate");
                glad_glBlendEquation =
                    (PFNGLBLENDEQUATIONPROC)load_opengl_func("glBlendEquation");
                glad_glBlendEquationSeparate =
                    (PFNGLBLENDEQUATIONSEPARATEPROC)load_opengl_func("glBlendEquationSeparate");
                glad_glBlendColor = (PFNGLBLENDCOLORPROC)load_opengl_func("glBlendColor");
                glad_glCullFace = (PFNGLCULLFACEPROC)load_opengl_func("glCullFace");
                glad_glFrontFace = (PFNGLFRONTFACEPROC)load_opengl_func("glFrontFace");
                glad_glPolygonOffset =
                    (PFNGLPOLYGONOFFSETPROC)load_opengl_func("glPolygonOffset");
                glad_glActiveTexture =
                    (PFNGLACTIVETEXTUREPROC)load_opengl_func("glActiveTexture");
                glad_glViewport = (PFNGLVIEWPORTPROC)load_opengl_func("glViewport");
                glad_glScissor = (PFNGLSCISSORPROC)load_opengl_func("glScissor");
                glad_glClear = (PFNGLCLEARPROC)load_opengl_func("glClear");
                glad_glClearColor = (PFNGLCLEARCOLORPROC)load_opengl_func("glClearColor");
                glad_glClearDepthf = (PFNGLCLEARDEPTHFPROC)load_opengl_func("glClearDepthf");
                glad_glClearStencil =
                    (PFNGLCLEARSTENCILPROC)load_opengl_func("glClearStencil");
                glad_glFinish = (PFNGLFINISHPROC)load_opengl_func("glFinish");
                glad_glFlush = (PFNGLFLUSHPROC)load_opengl_func("glFlush");
                glad_glHint = (PFNGLHINTPROC)load_opengl_func("glHint");
                glad_glLineWidth = (PFNGLLINEWIDTHPROC)load_opengl_func("glLineWidth");
                glad_glSampleCoverage =
                    (PFNGLSAMPLECOVERAGEPROC)load_opengl_func("glSampleCoverage");

                // Buffers
                glad_glGenBuffers = (PFNGLGENBUFFERSPROC)load_opengl_func("glGenBuffers");
                glad_glDeleteBuffers =
                    (PFNGLDELETEBUFFERSPROC)load_opengl_func("glDeleteBuffers");
                glad_glBindBuffer = (PFNGLBINDBUFFERPROC)load_opengl_func("glBindBuffer");
                glad_glBufferData = (PFNGLBUFFERDATAPROC)load_opengl_func("glBufferData");
                glad_glBufferSubData =
                    (PFNGLBUFFERSUBDATAPROC)load_opengl_func("glBufferSubData");
                glad_glIsBuffer = (PFNGLISBUFFERPROC)load_opengl_func("glIsBuffer");
                glad_glGetBufferParameteriv =
                    (PFNGLGETBUFFERPARAMETERIVPROC)load_opengl_func("glGetBufferParameteriv");

                // Textures
                glad_glGenTextures = (PFNGLGENTEXTURESPROC)load_opengl_func("glGenTextures");
                glad_glDeleteTextures =
                    (PFNGLDELETETEXTURESPROC)load_opengl_func("glDeleteTextures");
                glad_glBindTexture = (PFNGLBINDTEXTUREPROC)load_opengl_func("glBindTexture");
                glad_glIsTexture = (PFNGLISTEXTUREPROC)load_opengl_func("glIsTexture");
                glad_glTexImage2D = (PFNGLTEXIMAGE2DPROC)load_opengl_func("glTexImage2D");
                glad_glTexSubImage2D =
                    (PFNGLTEXSUBIMAGE2DPROC)load_opengl_func("glTexSubImage2D");
                glad_glCopyTexImage2D =
                    (PFNGLCOPYTEXIMAGE2DPROC)load_opengl_func("glCopyTexImage2D");
                glad_glCopyTexSubImage2D =
                    (PFNGLCOPYTEXSUBIMAGE2DPROC)load_opengl_func("glCopyTexSubImage2D");
                glad_glCompressedTexImage2D =
                    (PFNGLCOMPRESSEDTEXIMAGE2DPROC)load_opengl_func("glCompressedTexImage2D");
                glad_glCompressedTexSubImage2D =
                    (PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC)load_opengl_func(
                        "glCompressedTexSubImage2D");
                glad_glTexParameteri =
                    (PFNGLTEXPARAMETERIPROC)load_opengl_func("glTexParameteri");
                glad_glTexParameteriv =
                    (PFNGLTEXPARAMETERIVPROC)load_opengl_func("glTexParameteriv");
                glad_glTexParameterf =
                    (PFNGLTEXPARAMETERFPROC)load_opengl_func("glTexParameterf");
                glad_glTexParameterfv =
                    (PFNGLTEXPARAMETERFVPROC)load_opengl_func("glTexParameterfv");
                glad_glGenerateMipmap =
                    (PFNGLGENERATEMIPMAPPROC)load_opengl_func("glGenerateMipmap");
                glad_glPixelStorei = (PFNGLPIXELSTOREIPROC)load_opengl_func("glPixelStorei");
                glad_glReadPixels = (PFNGLREADPIXELSPROC)load_opengl_func("glReadPixels");

                // Framebuffers / renderbuffers
                glad_glGenFramebuffers =
                    (PFNGLGENFRAMEBUFFERSPROC)load_opengl_func("glGenFramebuffers");
                glad_glDeleteFramebuffers =
                    (PFNGLDELETEFRAMEBUFFERSPROC)load_opengl_func("glDeleteFramebuffers");
                glad_glBindFramebuffer =
                    (PFNGLBINDFRAMEBUFFERPROC)load_opengl_func("glBindFramebuffer");
                glad_glIsFramebuffer =
                    (PFNGLISFRAMEBUFFERPROC)load_opengl_func("glIsFramebuffer");
                glad_glCheckFramebufferStatus =
                    (PFNGLCHECKFRAMEBUFFERSTATUSPROC)load_opengl_func(
                        "glCheckFramebufferStatus");
                glad_glFramebufferTexture2D =
                    (PFNGLFRAMEBUFFERTEXTURE2DPROC)load_opengl_func("glFramebufferTexture2D");
                glad_glFramebufferRenderbuffer =
                    (PFNGLFRAMEBUFFERRENDERBUFFERPROC)load_opengl_func(
                        "glFramebufferRenderbuffer");
                glad_glGetFramebufferAttachmentParameteriv =
                    (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC)load_opengl_func(
                        "glGetFramebufferAttachmentParameteriv");
                glad_glGenRenderbuffers =
                    (PFNGLGENRENDERBUFFERSPROC)load_opengl_func("glGenRenderbuffers");
                glad_glDeleteRenderbuffers =
                    (PFNGLDELETERENDERBUFFERSPROC)load_opengl_func("glDeleteRenderbuffers");
                glad_glBindRenderbuffer =
                    (PFNGLBINDRENDERBUFFERPROC)load_opengl_func("glBindRenderbuffer");
                glad_glIsRenderbuffer =
                    (PFNGLISRENDERBUFFERPROC)load_opengl_func("glIsRenderbuffer");
                glad_glRenderbufferStorage =
                    (PFNGLRENDERBUFFERSTORAGEPROC)load_opengl_func("glRenderbufferStorage");
                glad_glGetRenderbufferParameteriv =
                    (PFNGLGETRENDERBUFFERPARAMETERIVPROC)load_opengl_func(
                        "glGetRenderbufferParameteriv");

                // Shaders / programs
                glad_glCreateShader =
                    (PFNGLCREATESHADERPROC)load_opengl_func("glCreateShader");
                glad_glDeleteShader =
                    (PFNGLDELETESHADERPROC)load_opengl_func("glDeleteShader");
                glad_glIsShader = (PFNGLISSHADERPROC)load_opengl_func("glIsShader");
                glad_glShaderSource =
                    (PFNGLSHADERSOURCEPROC)load_opengl_func("glShaderSource");
                glad_glCompileShader =
                    (PFNGLCOMPILESHADERPROC)load_opengl_func("glCompileShader");
                glad_glGetShaderiv = (PFNGLGETSHADERIVPROC)load_opengl_func("glGetShaderiv");
                glad_glGetShaderInfoLog =
                    (PFNGLGETSHADERINFOLOGPROC)load_opengl_func("glGetShaderInfoLog");
                glad_glGetShaderSource =
                    (PFNGLGETSHADERSOURCEPROC)load_opengl_func("glGetShaderSource");
                glad_glReleaseShaderCompiler =
                    (PFNGLRELEASESHADERCOMPILERPROC)load_opengl_func("glReleaseShaderCompiler");
                glad_glShaderBinary =
                    (PFNGLSHADERBINARYPROC)load_opengl_func("glShaderBinary");
                glad_glGetShaderPrecisionFormat =
                    (PFNGLGETSHADERPRECISIONFORMATPROC)load_opengl_func(
                        "glGetShaderPrecisionFormat");
                glad_glCreateProgram =
                    (PFNGLCREATEPROGRAMPROC)load_opengl_func("glCreateProgram");
                glad_glDeleteProgram =
                    (PFNGLDELETEPROGRAMPROC)load_opengl_func("glDeleteProgram");
                glad_glIsProgram = (PFNGLISPROGRAMPROC)load_opengl_func("glIsProgram");
                glad_glAttachShader =
                    (PFNGLATTACHSHADERPROC)load_opengl_func("glAttachShader");
                glad_glDetachShader =
                    (PFNGLDETACHSHADERPROC)load_opengl_func("glDetachShader");
                glad_glLinkProgram = (PFNGLLINKPROGRAMPROC)load_opengl_func("glLinkProgram");
                glad_glValidateProgram =
                    (PFNGLVALIDATEPROGRAMPROC)load_opengl_func("glValidateProgram");
                glad_glUseProgram = (PFNGLUSEPROGRAMPROC)load_opengl_func("glUseProgram");
                glad_glGetProgramiv =
                    (PFNGLGETPROGRAMIVPROC)load_opengl_func("glGetProgramiv");
                glad_glGetProgramInfoLog =
                    (PFNGLGETPROGRAMINFOLOGPROC)load_opengl_func("glGetProgramInfoLog");
                glad_glGetAttachedShaders =
                    (PFNGLGETATTACHEDSHADERSPROC)load_opengl_func("glGetAttachedShaders");
                glad_glBindAttribLocation =
                    (PFNGLBINDATTRIBLOCATIONPROC)load_opengl_func("glBindAttribLocation");
                glad_glGetAttribLocation =
                    (PFNGLGETATTRIBLOCATIONPROC)load_opengl_func("glGetAttribLocation");
                glad_glGetActiveAttrib =
                    (PFNGLGETACTIVEATTRIBPROC)load_opengl_func("glGetActiveAttrib");
                glad_glGetUniformLocation =
                    (PFNGLGETUNIFORMLOCATIONPROC)load_opengl_func("glGetUniformLocation");
                glad_glGetActiveUniform =
                    (PFNGLGETACTIVEUNIFORMPROC)load_opengl_func("glGetActiveUniform");
                glad_glGetUniformfv =
                    (PFNGLGETUNIFORMFVPROC)load_opengl_func("glGetUniformfv");
                glad_glGetUniformiv =
                    (PFNGLGETUNIFORMIVPROC)load_opengl_func("glGetUniformiv");

                // Uniforms
                glad_glUniform1f = (PFNGLUNIFORM1FPROC)load_opengl_func("glUniform1f");
                glad_glUniform2f = (PFNGLUNIFORM2FPROC)load_opengl_func("glUniform2f");
                glad_glUniform3f = (PFNGLUNIFORM3FPROC)load_opengl_func("glUniform3f");
                glad_glUniform4f = (PFNGLUNIFORM4FPROC)load_opengl_func("glUniform4f");
                glad_glUniform1i = (PFNGLUNIFORM1IPROC)load_opengl_func("glUniform1i");
                glad_glUniform2i = (PFNGLUNIFORM2IPROC)load_opengl_func("glUniform2i");
                glad_glUniform3i = (PFNGLUNIFORM3IPROC)load_opengl_func("glUniform3i");
                glad_glUniform4i = (PFNGLUNIFORM4IPROC)load_opengl_func("glUniform4i");
                glad_glUniform1fv = (PFNGLUNIFORM1FVPROC)load_opengl_func("glUniform1fv");
                glad_glUniform2fv = (PFNGLUNIFORM2FVPROC)load_opengl_func("glUniform2fv");
                glad_glUniform3fv = (PFNGLUNIFORM3FVPROC)load_opengl_func("glUniform3fv");
                glad_glUniform4fv = (PFNGLUNIFORM4FVPROC)load_opengl_func("glUniform4fv");
                glad_glUniform1iv = (PFNGLUNIFORM1IVPROC)load_opengl_func("glUniform1iv");
                glad_glUniform2iv = (PFNGLUNIFORM2IVPROC)load_opengl_func("glUniform2iv");
                glad_glUniform3iv = (PFNGLUNIFORM3IVPROC)load_opengl_func("glUniform3iv");
                glad_glUniform4iv = (PFNGLUNIFORM4IVPROC)load_opengl_func("glUniform4iv");
                glad_glUniformMatrix2fv =
                    (PFNGLUNIFORMMATRIX2FVPROC)load_opengl_func("glUniformMatrix2fv");
                glad_glUniformMatrix4fv =
                    (PFNGLUNIFORMMATRIX4FVPROC)load_opengl_func("glUniformMatrix4fv");

                // Vertex attributes
                glad_glVertexAttrib1f =
                    (PFNGLVERTEXATTRIB1FPROC)load_opengl_func("glVertexAttrib1f");
                glad_glVertexAttrib2f =
                    (PFNGLVERTEXATTRIB2FPROC)load_opengl_func("glVertexAttrib2f");
                glad_glVertexAttrib3f =
                    (PFNGLVERTEXATTRIB3FPROC)load_opengl_func("glVertexAttrib3f");
                glad_glVertexAttrib4f =
                    (PFNGLVERTEXATTRIB4FPROC)load_opengl_func("glVertexAttrib4f");
                glad_glVertexAttrib1fv =
                    (PFNGLVERTEXATTRIB1FVPROC)load_opengl_func("glVertexAttrib1fv");
                glad_glVertexAttrib2fv =
                    (PFNGLVERTEXATTRIB2FVPROC)load_opengl_func("glVertexAttrib2fv");
                glad_glVertexAttrib3fv =
                    (PFNGLVERTEXATTRIB3FVPROC)load_opengl_func("glVertexAttrib3fv");
                glad_glVertexAttrib4fv =
                    (PFNGLVERTEXATTRIB4FVPROC)load_opengl_func("glVertexAttrib4fv");
                glad_glVertexAttribPointer =
                    (PFNGLVERTEXATTRIBPOINTERPROC)load_opengl_func("glVertexAttribPointer");
                glad_glEnableVertexAttribArray =
                    (PFNGLENABLEVERTEXATTRIBARRAYPROC)load_opengl_func(
                        "glEnableVertexAttribArray");
                glad_glDisableVertexAttribArray =
                    (PFNGLDISABLEVERTEXATTRIBARRAYPROC)load_opengl_func(
                        "glDisableVertexAttribArray");
                glad_glGetVertexAttribfv =
                    (PFNGLGETVERTEXATTRIBFVPROC)load_opengl_func("glGetVertexAttribfv");
                glad_glGetVertexAttribiv =
                    (PFNGLGETVERTEXATTRIBIVPROC)load_opengl_func("glGetVertexAttribiv");
                glad_glGetVertexAttribPointerv =
                    (PFNGLGETVERTEXATTRIBPOINTERVPROC)load_opengl_func(
                        "glGetVertexAttribPointerv");

                // Drawing
                glad_glDrawArrays = (PFNGLDRAWARRAYSPROC)load_opengl_func("glDrawArrays");
                glad_glDrawElements =
                    (PFNGLDRAWELEMENTSPROC)load_opengl_func("glDrawElements");

                // Misc state queries
                glad_glGetError = (PFNGLGETERRORPROC)load_opengl_func("glGetError");
                glad_glGetString = (PFNGLGETSTRINGPROC)load_opengl_func("glGetString");
                glad_glGetIntegerv = (PFNGLGETINTEGERVPROC)load_opengl_func("glGetIntegerv");
                glad_glGetFloatv = (PFNGLGETFLOATVPROC)load_opengl_func("glGetFloatv");
                glad_glGetBooleanv = (PFNGLGETBOOLEANVPROC)load_opengl_func("glGetBooleanv");
                glad_glGetTexParameteriv =
                    (PFNGLGETTEXPARAMETERIVPROC)load_opengl_func("glGetTexParameteriv");
                glad_glGetTexParameterfv =
                    (PFNGLGETTEXPARAMETERFVPROC)load_opengl_func("glGetTexParameterfv");
            }
#endif
        } else {
            // Else, try to load them on our own
            if (!gladLoadGL()) {
                LOG_CRITICAL(Frontend, "Glad failed to load (internal symbols)!");
                return;
            }
        }
        break;
#endif
#ifdef ENABLE_VULKAN
    case Settings::GraphicsAPI::Vulkan:
        LibRetro::VulkanResetContext();
        break;
#endif
    default:
        // software renderer never gets here
        break;
    }

    emu_instance->emu_window->CreateContext();

    if (!emu_instance->game_loaded) {
        emu_instance->game_loaded = do_load_game();
    } else {
        // Game is already loaded, just recreate the renderer for the new GL context
        if (Settings::values.graphics_api.GetValue() == Settings::GraphicsAPI::OpenGL) {
            Core::System::GetInstance().GPU().RecreateRenderer(*emu_instance->emu_window, nullptr);
        }
    }
}

static void context_destroy() {
    LOG_DEBUG(Frontend, "context_destroy");
    if (emu_instance->game_loaded &&
        Settings::values.graphics_api.GetValue() == Settings::GraphicsAPI::OpenGL) {
        // Release the renderer's OpenGL resources
        Core::System::GetInstance().GPU().ReleaseRenderer();
    }
    emu_instance->emu_window->DestroyContext();
}

void retro_reset() {
    LOG_DEBUG(Frontend, "retro_reset");
    Core::System::GetInstance().Shutdown();
    emu_instance->game_loaded = do_load_game();
}

/**
 * libretro callback; Called when a game is to be loaded.
 */
bool retro_load_game(const struct retro_game_info* info) {
    LOG_INFO(Frontend, "Starting Azahar RetroArch game...");

#if CITRA_ARCH(x86_64) && CITRA_HAS_SSE42
    if (!Common::GetCPUCaps().sse4_2) {
        LOG_CRITICAL(Frontend, "This CPU does not support SSE4.2, which is required by this build");
        LibRetro::DisplayMessage(
            "This CPU does not support SSE4.2, which is required by this build");
        return false;
    }
#endif

    UpdateSettings();

    // If using HW rendering, don't actually load the game here. azahar wants
    // the graphics context ready and available before calling System::Load.
    LibRetro::settings.file_path = info->path;

    if (!LibRetro::SetPixelFormat(RETRO_PIXEL_FORMAT_XRGB8888)) {
        LibRetro::DisplayMessage("XRGB8888 is not supported.");
        return false;
    }

    emu_instance->emu_window->UpdateLayout();

    switch (Settings::values.graphics_api.GetValue()) {
    case Settings::GraphicsAPI::OpenGL:
#ifdef ENABLE_OPENGL
        LOG_INFO(Frontend, "Using OpenGL hw renderer");
        LibRetro::SetHWSharedContext();
#if defined(USING_GLES) || defined(__EMSCRIPTEN__)
        emu_instance->hw_render.context_type = RETRO_HW_CONTEXT_OPENGLES3;
        emu_instance->hw_render.version_major = 3;
        emu_instance->hw_render.version_minor = 0;
#else
        emu_instance->hw_render.context_type = RETRO_HW_CONTEXT_OPENGL_CORE;
        emu_instance->hw_render.version_major = 4;
        emu_instance->hw_render.version_minor = 3;
#endif
        emu_instance->hw_render.context_reset = context_reset;
        emu_instance->hw_render.context_destroy = context_destroy;
        emu_instance->hw_render.cache_context = false;
        emu_instance->hw_render.bottom_left_origin = true;
        if (!LibRetro::SetHWRenderer(&emu_instance->hw_render)) {
            LibRetro::DisplayMessage("Failed to set HW renderer");
            return false;
        }
#endif
        break;
    case Settings::GraphicsAPI::Vulkan:
#ifdef ENABLE_VULKAN
        LOG_INFO(Frontend, "Using Vulkan hw renderer");
        emu_instance->hw_render.context_type = RETRO_HW_CONTEXT_VULKAN;
        emu_instance->hw_render.version_major = VK_MAKE_VERSION(1, 1, 0);
        emu_instance->hw_render.version_minor = 0;
        emu_instance->hw_render.context_reset = context_reset;
        emu_instance->hw_render.context_destroy = context_destroy;
        emu_instance->hw_render.cache_context = true;
        if (!LibRetro::SetHWRenderer(&emu_instance->hw_render)) {
            LibRetro::DisplayMessage("Failed to set HW renderer");
            return false;
        }

        // Set up Vulkan context negotiation interface
        static const struct retro_hw_render_context_negotiation_interface_vulkan vk_negotiation = {
            RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN,
            RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN_VERSION,
            LibRetro::GetVulkanApplicationInfo,
            LibRetro::CreateVulkanDevice,
            nullptr, // destroy_device - not needed (frontend owns the device)
        };
        LibRetro::SetHWRenderContextNegotiationInterface((void**)&vk_negotiation);
#endif
        break;
    case Settings::GraphicsAPI::Software:
        emu_instance->game_loaded = do_load_game();
        if (!emu_instance->game_loaded)
            return false;
        break;
    }

    uint64_t quirks =
        RETRO_SERIALIZATION_QUIRK_CORE_VARIABLE_SIZE | RETRO_SERIALIZATION_QUIRK_MUST_INITIALIZE;
    LibRetro::SetSerializationQuirks(quirks);

    return true;
}

void retro_unload_game() {
    LOG_DEBUG(Frontend, "Unloading game...");
    Core::System::GetInstance().Shutdown();
}

unsigned retro_get_region() {
    return RETRO_REGION_NTSC;
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info* info,
                             size_t num_info) {
    return retro_load_game(info);
}

/// Drain any pending async kernel operations by running the emulation loop.
///
/// Savestates are unsafe to create while RunAsync operations (file I/O, network, etc.)
/// are in flight. The Qt frontend handles this by deferring serialization inside
/// System::RunLoop(): it sets a request flag via SendSignal(Signal::Save), and RunLoop
/// only performs the save when !kernel->AreAsyncOperationsPending() (see core.cpp).
///
/// The Qt frontend needs that indirection because its UI and emulation run on separate
/// threads. In libretro, the frontend calls API entry points (retro_run, retro_serialize,
/// etc.) sequentially, so we can call RunLoop() directly from here to drain pending ops,
/// then call SaveStateBuffer()/LoadStateBuffer() ourselves.
///
/// Note: RunLoop() can itself start new async operations (CPU executes HLE service calls),
/// so the pending count may not decrease monotonically. In practice games reach quiescent
/// points between frames; the 5-second timeout (matching RunLoop's existing handler)
/// covers the pathological case.
static bool DrainAsyncOperations(Core::System& system) {
    if (!system.KernelRunning() || !system.Kernel().AreAsyncOperationsPending()) {
        return true;
    }

    emu_instance->emu_window->suppressPresentation = true;
    auto start = std::chrono::steady_clock::now();

    while (system.Kernel().AreAsyncOperationsPending()) {
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(5)) {
            LOG_ERROR(Frontend, "Timed out waiting for async operations to complete");
            emu_instance->emu_window->suppressPresentation = false;
            return false;
        }
        auto result = system.RunLoop();
        if (result != Core::System::ResultStatus::Success) {
            emu_instance->emu_window->suppressPresentation = false;
            return false;
        }
    }

    emu_instance->emu_window->suppressPresentation = false;
    return true;
}

std::optional<std::vector<u8>> savestate = {};

size_t retro_serialize_size() {
    auto& system = Core::System::GetInstance();
    if (!system.IsPoweredOn())
        return 0;

    if (!DrainAsyncOperations(system)) {
        savestate.reset();
        return 0;
    }

    try {
        savestate = system.SaveStateBuffer();
        return savestate->size();
    } catch (const std::exception& e) {
        LOG_ERROR(Frontend, "Error saving state: {}", e.what());
        savestate.reset();
        return 0;
    }
}

bool retro_serialize(void* data, size_t size) {
    if (!savestate.has_value())
        return false;
    if (size < savestate->size())
        return false;
    memcpy(data, savestate->data(), savestate->size());
    savestate.reset();
    return true;
}

bool retro_unserialize(const void* data, size_t size) {
    auto& system = Core::System::GetInstance();
    if (!system.IsPoweredOn())
        return false;

    if (!DrainAsyncOperations(system)) {
        return false;
    }

    std::vector<u8> buffer(static_cast<const u8*>(data), static_cast<const u8*>(data) + size);
    try {
        return system.LoadStateBuffer(std::move(buffer));
    } catch (const std::exception& e) {
        LOG_ERROR(Frontend, "Error loading state: {}", e.what());
        return false;
    }
}

void* retro_get_memory_data(unsigned id) {
    // Memory is exposed via RETRO_ENVIRONMENT_SET_MEMORY_MAPS instead,
    // using virtual addresses for stable cheat/achievement support.
    return NULL;
}

size_t retro_get_memory_size(unsigned id) {
    return 0;
}

void retro_cheat_reset() {}

void retro_cheat_set(unsigned index, bool enabled, const char* code) {}
