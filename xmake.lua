add_rules("mode.debug", "mode.release")

add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")

option("levilamina_version")
    set_showmenu(true)
    set_default("26.20.4")
    set_values("26.10.14", "26.20.4")
    set_description("LeviLamina client SDK version used for the mod target")
option_end()

local levilamina_version = get_config("levilamina_version") or "26.20.4"

add_requires("levilamina " .. levilamina_version, {configs = {target_type = "client"}})

add_requires("levibuildscript")
add_requires("imgui", {configs = {shared = false, win32 = true, dx11 = true}})
add_requires("minhook", {configs = {shared = false}})
add_requires("nlohmann_json")

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

target("ChiyanMap")
    add_rules("@levibuildscript/linkrule")
    add_rules("@levibuildscript/modpacker")
    add_cxflags(
        "/EHa",
        "/utf-8",
        "/W4",
        "/w44265",
        "/w44289",
        "/w44296",
        "/w45263",
        "/w44738",
        "/w45204",
        "/wd4100",
        "/wd4189"
    )
    add_defines("NOMINMAX", "UNICODE")
    add_packages("levilamina", "imgui", "minhook", "nlohmann_json")
    add_syslinks("d3d11", "d3d12", "dxgi", "windowscodecs", "ole32", "user32", "runtimeobject", "windowsapp", "delayimp")
    add_ldflags("/DELAYLOAD:dwmapi.dll", "/DELAYLOAD:imm32.dll", "/DELAYLOAD:LeviLamina.dll")
    add_shflags("/DELAYLOAD:dwmapi.dll", "/DELAYLOAD:imm32.dll", "/DELAYLOAD:LeviLamina.dll")
    set_kind("shared")
    set_languages("c++20")
    set_symbols("debug")

    add_headerfiles("src/**.h")
    add_files("src/**.cpp")
    add_files(
        "third_party/cubiomes-bedrock/biomenoise.c",
        "third_party/cubiomes-bedrock/biomes.c",
        "third_party/cubiomes-bedrock/cave.c",
        "third_party/cubiomes-bedrock/finders.c",
        "third_party/cubiomes-bedrock/generator.c",
        "third_party/cubiomes-bedrock/layers.c",
        "third_party/cubiomes-bedrock/mt.c",
        "third_party/cubiomes-bedrock/noise.c",
        "third_party/cubiomes-bedrock/quadbase.c",
        "third_party/cubiomes-bedrock/util.c"
    )
    add_includedirs("src")
    add_includedirs("third_party/cubiomes-bedrock")
    add_defines("_USE_MATH_DEFINES")
    add_cflags("/D__attribute__(x)=", {tools = {"cl"}})

target("worldgen_tests")
    set_kind("binary")
    set_default(false)
    set_languages("c++20")
    set_symbols("debug")
    add_defines("CHIYANMAP_WORLDGEN_TEST_MAIN")
    add_includedirs("src", "third_party/cubiomes-bedrock")
    add_files("src/worldgen/**.cpp", "src/state/SeedMapManager.cpp", "tests/worldgen/**.cpp")
    add_files(
        "third_party/cubiomes-bedrock/biomenoise.c",
        "third_party/cubiomes-bedrock/biomes.c",
        "third_party/cubiomes-bedrock/cave.c",
        "third_party/cubiomes-bedrock/finders.c",
        "third_party/cubiomes-bedrock/generator.c",
        "third_party/cubiomes-bedrock/layers.c",
        "third_party/cubiomes-bedrock/mt.c",
        "third_party/cubiomes-bedrock/noise.c",
        "third_party/cubiomes-bedrock/quadbase.c",
        "third_party/cubiomes-bedrock/util.c"
    )
