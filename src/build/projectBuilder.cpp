/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "projectBuilder.h"

#include <filesystem>
#include <thread>
#include <algorithm>
#include "../utils/fs.h"
#include "../utils/logger.h"
#include "../utils/proc.h"
#include "../utils/string.h"
#include "../utils/textureFormats.h"
#include "../utils/toolchain.h"
#include "romMetaBuilder.h"
#include "../editor/imgui/notification.h"
#include <cstdlib>
#include <cctype>

namespace fs = std::filesystem;
using AT = Project::FileType;

namespace
{
  struct AssetBuilder
  {
    Build::BuildFunc func;
    const char* name;
  };

  constexpr auto assetBuilders = std::to_array<AssetBuilder>({
    {Build::buildT3DMAssets,    "3D Model"},
    {Build::buildFontAssets,    "Font"},
    {Build::buildTextureAssets, "Texture"},
    {Build::buildAudioAssets,   "Audio"},
    // must be last: (@TODO: handle prefab referencing prefab, [not in the editor yet])
    {Build::buildPrefabAssets,  "Prefab"},
  });
}

void Build::SceneCtx::addAsset(const Project::AssetManagerEntry &entry)
{
  assetUUIDToIdx[entry.getUUID()] = assetList.size();
  if(entry.romPath.size() > 5) {
    auto outNameNoPrefix = entry.romPath.substr(5); // remove "rom:/"
    assetFileMap += "if(path == \"" + outNameNoPrefix + "\")return " + std::to_string(assetList.size()) + ";\n";
  }

  uint32_t flags = 0;
  if(entry.type == AT::FONT) {
    flags |= 0x01; // KEEP_LOADED
  }

  assetList.push_back({entry.romPath, stringOffset, (uint32_t)entry.type, flags});
  stringOffset += entry.romPath.size() + 1;
}

bool Build::buildProject(const std::string &configPath, bool runN64Make)
{
  Project::Project project{configPath};
  auto path = project.getPath();
  Utils::Logger::log("Building project...");

  if(project.conf.pathN64Inst.empty())
  {
    // read N64_INST from environment
    char* n64InstEnv = getenv("N64_INST");
    if(n64InstEnv != nullptr) {
      project.conf.pathN64Inst = n64InstEnv;
    }
  // On Windows, fall back to pyrite64-sdk (autoinstaller location) if not explicitly set by the user. 
  #if defined(_WIN32)
    else {
      project.conf.pathN64Inst = "/pyrite64-sdk";
    }
  #endif
  } else {
  #if defined(_WIN32)
    //_putenv_s("N64_INST", project.conf.pathN64Inst.c_str());
  #else
    setenv("N64_INST", project.conf.pathN64Inst.c_str(), 0);
  #endif
  }

  #if defined(_WIN32)
    _putenv_s("N64_INST", project.conf.pathN64Inst.c_str());
    _putenv_s("MSYSTEM", "MINGW64");
    _putenv_s("PATH", "C:\\msys64\\mingw64\\bin;C:\\msys64\\usr\\bin");
  #endif

  auto fsPath = fs::absolute(fs::path{path} / "filesystem");
  auto fsDataPath = fs::absolute(fs::path{path} / "filesystem" / "p64");
  if (!fs::exists(fsDataPath)) {
    fs::create_directories(fsDataPath);
  }

  SceneCtx sceneCtx{};
  sceneCtx.toolchain.scan();
  sceneCtx.project = &project;

  // Global project config
  sceneCtx.files.push_back("filesystem/p64/conf");

  // Asset-Manager
  for (auto &typed : project.getAssets().getEntries()) {
    for (auto &entry : typed) {
      if (entry.conf.exclude || entry.type == Project::FileType::UNKNOWN
        || entry.type == Project::FileType::CODE_OBJ
        || entry.type == Project::FileType::CODE_GLOBAL
      ) continue;
      sceneCtx.addAsset(entry);
    }
  }

  // check if files got added/removed, in which case trigger an asset rebuild.
  // This is needed as some assets reference others via indices.
  {
    auto fileNames = sceneCtx.files;
    std::sort(fileNames.begin(), fileNames.end());
    auto fileStr = Utils::join(fileNames, " ");

    auto oldFileStr = Utils::FS::loadTextFile(fsDataPath / "fileList.txt");
    if(fileStr != oldFileStr)
    {
      Utils::Logger::log("Asset list changed, clean asset dependencies");
      Utils::FS::delTypeRecursive(fsPath, ".t3dm");
      Utils::FS::delTypeRecursive(fsPath, ".pf");
      fs::remove_all(fsDataPath);
      fs::create_directories(fsDataPath);

      cleanProject(project, {
        .code = true,
        .assets = false,
        .engine = false,
      });

      Utils::FS::saveTextFile(fsDataPath / "fileList.txt", fileStr);
    }
  }

  // User scripts
  auto userDirs = Utils::FS::scanDirs(path + "/src/user");
  std::string userCodeRules = "";
  for (const auto &dir : userDirs) {
    userCodeRules += "src += $(wildcard src/user/" + dir + "/*.cpp)\n";
  }

  if(!buildNodeGraphAssets(project, sceneCtx)) {
    Utils::Logger::log(std::string("Graph-Asset build failed!", Utils::Logger::LEVEL_ERROR));
    return false;
  }

  // Scripts
  buildGlobalScripts(project, sceneCtx);
  buildScripts(project, sceneCtx);

  // Scenes
  project.getScenes().reload();
  const auto &scenes = project.getScenes().getEntries();

  std::string sceneMapStr{};
  std::string sceneNameStr{};
  for (const auto &scene : scenes) {
    sceneMapStr += "if(path == \"" + scene.name + "\")return " + std::to_string(scene.id) + ";\n";
    sceneNameStr += "\"" + scene.name + "\",\n";
    try
    {
      buildScene(project, scene, sceneCtx);
    } catch(const std::exception &e)
    {
      auto msg = std::string("Scene build failed:\n") + e.what();
      Utils::Logger::log(msg, Utils::Logger::LEVEL_ERROR);
      Editor::Noti::add(Editor::Noti::Type::ERROR, msg);
      return false;
    }
  }

  auto sceneTableHeader = Utils::replaceAll(Utils::FS::loadTextFile("data/scripts/sceneTable.h"), {
    {"{{SCENE_MAP}}", sceneMapStr},
    {"{{SCENE_COUNT}}", std::to_string(scenes.size())}
  });
  Utils::FS::saveTextFile(project.getPath() + "/src/p64/sceneTable.h", sceneTableHeader);

  Utils::FS::saveTextFile(project.getPath() + "/src/p64/sceneTable.cpp",
    "#include \"sceneTable.h\"\n"
    "\n"
    "namespace P64::SceneManager {\n"
    "  const char* SCENE_NAMES["+std::to_string(scenes.size())+"] = {\n" + sceneNameStr + "};\n"
    "}\n"
  );


  for(auto &builder : assetBuilders)
  {
    if(!builder.func(project, sceneCtx)) {
      Utils::Logger::log(std::string(builder.name) + " Asset build failed!", Utils::Logger::LEVEL_ERROR);
      return false;
    }
  }

  auto assetTableCode = Utils::replaceAll(
    Utils::FS::loadTextFile("data/scripts/assetTable.h"),
    "{{ASSET_MAP}}", sceneCtx.assetFileMap
  );
  Utils::FS::saveTextFile(project.getPath() + "/src/p64/assetTable.h", assetTableCode);

  // Asset table
  Utils::BinaryFile fileList{};
  fileList.write<uint32_t>(sceneCtx.assetList.size());
  uint32_t baseOffset = (sceneCtx.assetList.size() * sizeof(uint32_t)*2) + sizeof(uint32_t);
  for (auto &entry : sceneCtx.assetList) {
    fileList.write(baseOffset + entry.stringOffset);
    uint32_t ptr = entry.type << (32-4);
    ptr |= entry.flags << (32-8);
    fileList.write(ptr);
  }
  for (auto &entry : sceneCtx.assetList) {
    fileList.writeChars(entry.path.c_str(), entry.path.size()+1);
  }
  fileList.writeToFile(fsDataPath / "a");

  // kep order stable to detect makefile changes
  auto filesSorted = sceneCtx.files;
  std::sort(filesSorted.begin(), filesSorted.end());

  // Makefile
  auto makefile = Utils::replaceAll(
    Utils::FS::loadTextFile("data/build/baseMakefile.mk"),
    {
      {"{{N64_INST}}",          project.conf.pathN64Inst},
      {"{{ROM_NAME}}",          project.conf.romName},
      {"{{PROJECT_NAME}}",      project.conf.name},
      {"{{ASSET_LIST}}",        Utils::join(filesSorted, " ")},
      {"{{USER_CODE_DIRS}}",    userCodeRules},
      {"{{ROM_HEADER_FLAGS}}",  buildRomHeaderFlags(project)},
      {"{{P64_SELF_PATH}}",     Utils::Proc::getSelfPath().string()},
      {"{{PROJECT_SELF_PATH}}", fs::absolute(configPath).string()},
    }
  );

  auto mkPath = fs::absolute(path) / "Makefile";
  Utils::FS::saveTextFile(mkPath, makefile);

  {
    Utils::BinaryFile f{};
    f.write<uint32_t>(project.conf.sceneIdOnBoot);
    f.write<uint32_t>(project.conf.sceneIdOnReset);
    for(uint32_t i=0; i<sceneCtx.autoLoadFontUUIDs.size(); ++i) {
      auto uuid = sceneCtx.autoLoadFontUUIDs[i];
      f.write<uint16_t>(uuid == 0 ? 0xFFFF : sceneCtx.assetUUIDToIdx[uuid]);
    }
    f.writeToFile(fsDataPath / "conf");
  }

  // Build
  bool success = true;
  if (runN64Make) {
    success = sceneCtx.toolchain.runCmdSyncLogged("make -C \"" + path + "\" -j8");
  }

  if(success) {
    Utils::Logger::log(runN64Make ? "Build done!" : "Project data ready.");
  } else {
    Utils::Logger::log("Build failed!", Utils::Logger::LEVEL_ERROR);
  }
  return success;
}

bool Build::buildProjectPC(const std::string &configPath)
{
#if !defined(_WIN32)
  Utils::Logger::log("Build for PC is currently supported on Windows only.", Utils::Logger::LEVEL_ERROR);
  return false;
#else
  if (!buildProject(configPath, false)) {
    return false;
  }

  Project::Project project{configPath};
  auto path = project.getPath();
  fs::path buildPcDir = fs::path{path} / "build-pc";
  fs::create_directories(buildPcDir);

  // Resolve pyrite64 root (editor executable's directory or repo root when run from build)
  fs::path pyrite64Root = Utils::Proc::getAppResourcePath();
  if (pyrite64Root.empty() || !fs::exists(pyrite64Root / "data")) {
    pyrite64Root = Utils::Proc::getSelfPath().parent_path();
  }
  for (int i = 0; i < 5 && !fs::exists(pyrite64Root / "data" / "build" / "CMakeLists.pc.template"); ++i) {
    if (pyrite64Root.has_parent_path())
      pyrite64Root = pyrite64Root.parent_path();
    else
      break;
  }
  if (!fs::exists(pyrite64Root / "data" / "build" / "CMakeLists.pc.template")) {
    Utils::Logger::log("PC build template not found. Ensure data/build/CMakeLists.pc.template exists.", Utils::Logger::LEVEL_ERROR);
    return false;
  }

  auto templateStr = Utils::FS::loadTextFile((pyrite64Root / "data" / "build" / "CMakeLists.pc.template").string());
  auto cmakeLists = Utils::replaceAll(templateStr, {
    {"{{PYRITE64_ROOT}}", pyrite64Root.generic_string()},
    {"{{PROJECT_PATH}}",  fs::absolute(path).generic_string()},
    {"{{ROM_NAME}}",     project.conf.romName},
    {"{{PROJECT_NAME}}",  project.conf.name},
  });
  Utils::FS::saveTextFile((buildPcDir / "CMakeLists.txt").string(), cmakeLists);
  /* Lets the PC .exe find assets when the compile-time path is wrong or the project was moved. */
  Utils::FS::saveTextFile((buildPcDir / "p64_project_root.txt").string(),
                          fs::absolute(path).generic_string() + "\n");

  // Prepend MSYS2 toolchain to PATH so cmake/ninja are found when editor is not launched from MSYS2 shell
  const char* oldPathEnv = std::getenv("PATH");
  std::string oldPath = oldPathEnv ? oldPathEnv : "";
  fs::path toolPath;
  if (fs::exists(fs::path("C:/msys64/ucrt64/bin/cmake.exe")))
    toolPath = "C:/msys64/ucrt64/bin;C:/msys64/usr/bin";
  else if (fs::exists(fs::path("C:/msys64/mingw64/bin/cmake.exe")))
    toolPath = "C:/msys64/mingw64/bin;C:/msys64/usr/bin";
  if (!toolPath.empty()) {
    std::string newPath = toolPath.generic_string() + ";" + oldPath;
    _putenv_s("PATH", newPath.c_str());
  }

  Utils::Logger::log("Configuring PC build (GLES2)...");
  std::string configureCmd = "cmake -S \"" + buildPcDir.string() + "\" -B \"" + buildPcDir.string() + "\" -G \"MinGW Makefiles\" -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=mingw32-make";
  if (!Utils::Proc::runSyncLogged(configureCmd)) {
    Utils::Logger::log("Trying Ninja...", Utils::Logger::LEVEL_WARN);
    configureCmd = "cmake -S \"" + buildPcDir.string() + "\" -B \"" + buildPcDir.string() + "\" -G \"Ninja\" -DCMAKE_BUILD_TYPE=Release";
    if (!Utils::Proc::runSyncLogged(configureCmd)) {
      Utils::Logger::log("PC configure failed. Install MinGW (msys2/mingw64) or Ninja.", Utils::Logger::LEVEL_ERROR);
      return false;
    }
  }

  Utils::Logger::log("Building PC executable...");
  std::string buildCmd = "cmake --build \"" + buildPcDir.string() + "\" --config Release";
  bool success = Utils::Proc::runSyncLogged(buildCmd);

  if (!toolPath.empty())
    _putenv_s("PATH", oldPath.c_str());

  if (success) {
    Utils::Logger::log("PC build done! Output in project build-pc/");
  } else {
    Utils::Logger::log("PC build failed!", Utils::Logger::LEVEL_ERROR);
  }
  return success;
#endif
}

bool Build::buildProjectDC(const std::string &configPath)
{
  if (!buildProject(configPath, false)) {
    return false;
  }

  Project::Project project{configPath};
  auto path = project.getPath();
  fs::path buildDcDir = fs::path{path} / "build-dc";
  fs::create_directories(buildDcDir);

  fs::path pyrite64Root = Utils::Proc::getAppResourcePath();
  if (pyrite64Root.empty() || !fs::exists(pyrite64Root / "data")) {
    pyrite64Root = Utils::Proc::getSelfPath().parent_path();
  }
  for (int i = 0; i < 5 && !fs::exists(pyrite64Root / "data" / "build" / "Makefile.dc.template"); ++i) {
    if (pyrite64Root.has_parent_path())
      pyrite64Root = pyrite64Root.parent_path();
    else
      break;
  }
  if (!fs::exists(pyrite64Root / "data" / "build" / "Makefile.dc.template")) {
    Utils::Logger::log("Dreamcast build template not found. Ensure data/build/Makefile.dc.template exists.", Utils::Logger::LEVEL_ERROR);
    return false;
  }

  /* SH-4 kos-c++ only resolves MSYS paths (/c/...), not Windows C:/... */
  auto toMsysPath = [](std::string s) {
    for (char& c : s) if (c == '\\') c = '/';
    while (s.size() > 1 && s.back() == '/') s.pop_back();
    if (s.size() >= 2 && std::isalpha(static_cast<unsigned char>(s[0])) && s[1] == ':') {
      char drive = static_cast<char>(std::tolower(static_cast<unsigned char>(s[0])));
      s = std::string("/") + drive + s.substr(2);
    }
    return s;
  };

  auto templateStr = Utils::FS::loadTextFile((pyrite64Root / "data" / "build" / "Makefile.dc.template").string());
  auto makefile = Utils::replaceAll(templateStr, {
    {"{{PYRITE64_ROOT}}", toMsysPath(pyrite64Root.generic_string())},
    {"{{PROJECT_PATH}}",  toMsysPath(fs::absolute(path).generic_string())},
    {"{{ROM_NAME}}",     project.conf.romName},
    {"{{PROJECT_NAME}}",  project.conf.name},
  });
  Utils::FS::saveTextFile((buildDcDir / "Makefile").string(), makefile);
  Utils::FS::saveTextFile((buildDcDir / "p64_project_root.txt").string(),
                          fs::absolute(path).generic_string() + "\n");

  fs::path runtimeDc = pyrite64Root / "n64" / "runtime_dc";
  if (!fs::exists(runtimeDc / "Makefile")) {
    Utils::Logger::log("Dreamcast runtime missing (n64/runtime_dc/Makefile).", Utils::Logger::LEVEL_ERROR);
    return false;
  }

  auto findKosBase = []() -> fs::path {
    if (const char* env = std::getenv("KOS_BASE"); env && env[0] && fs::exists(env))
      return fs::path{env};
    const char* candidates[] = {
      "/opt/toolchains/dc/kos",
      "C:/msys64/opt/toolchains/dc/kos",
      "C:/opt/toolchains/dc/kos",
      "C:/dreamcast/kos",
      "C:/kos",
    };
    for (const char* c : candidates) {
      fs::path p{c};
      if (fs::exists(p / "environ.sh") || fs::exists(p / "Makefile"))
        return p;
    }
    // Scan MSYS home/<user>/kos directories
    fs::path msysHome{"C:/msys64/home"};
    if (fs::exists(msysHome)) {
      for (auto& ent : fs::directory_iterator(msysHome)) {
        if (!ent.is_directory()) continue;
        for (const char* name : {"kos", "KallistiOS", "dreamcast/kos"}) {
          fs::path p = ent.path() / name;
          if (fs::exists(p / "environ.sh"))
            return p;
        }
      }
    }
    return {};
  };

  fs::path kosBase = findKosBase();
  if (kosBase.empty()) {
    Utils::Logger::log(
      "KallistiOS not found. Install KOS and set KOS_BASE (or place it at /opt/toolchains/dc/kos).",
      Utils::Logger::LEVEL_ERROR);
    return false;
  }
  Utils::Logger::log("Using KallistiOS at " + kosBase.generic_string());

  /* Newer clones ship doc/environ.sh.sample; install may not have copied it yet. */
  if (!fs::exists(kosBase / "environ.sh")) {
    fs::path sample = kosBase / "doc" / "environ.sh.sample";
    if (fs::exists(sample)) {
      std::error_code ec;
      fs::copy_file(sample, kosBase / "environ.sh", ec);
      if (ec) {
        Utils::Logger::log("Failed to create environ.sh from sample: " + ec.message(), Utils::Logger::LEVEL_ERROR);
        return false;
      }
      Utils::Logger::log("Created environ.sh from doc/environ.sh.sample");
    } else {
      Utils::Logger::log(
        "Missing environ.sh in KOS_BASE. Finish install_kos.bat (or copy doc/environ.sh.sample to environ.sh).",
        Utils::Logger::LEVEL_ERROR);
      return false;
    }
  }

  fs::path shElfGcc = kosBase.parent_path() / "sh-elf" / "bin" / "sh-elf-gcc.exe";
  fs::path shElfGccUnix = kosBase.parent_path() / "sh-elf" / "bin" / "sh-elf-gcc";
  if (!fs::exists(shElfGcc) && !fs::exists(shElfGccUnix)) {
    Utils::Logger::log(
      "Dreamcast SH-4 toolchain not found (expected next to KOS at "
      + (kosBase.parent_path() / "sh-elf").generic_string()
      + "). Run install_kos.bat and wait for dc-chain to finish — that step can take 1-3+ hours.",
      Utils::Logger::LEVEL_ERROR);
    return false;
  }

  std::string kosUnix = toMsysPath(kosBase.generic_string());
  std::string buildUnix = toMsysPath(fs::absolute(buildDcDir).generic_string());
  std::string environSh = kosUnix + "/environ.sh";

  Utils::Logger::log("Building Dreamcast ELF/CDI (KallistiOS)...");
  /* toolchain.runCmdSyncLogged wraps in bash -lc '...', so avoid single quotes and $(...) inside. */
  std::string dcBin = toMsysPath((kosBase.parent_path() / "bin").generic_string());
  std::string inner =
    "set -e; "
    "export KOS_BASE=\"" + kosUnix + "\"; "
    "source \"" + environSh + "\"; "
    "export PATH=\"" + dcBin + ":$PATH\"; "
    "make -C \"" + buildUnix + "\" -j4";

  Utils::Toolchain toolchain{};
  toolchain.scan();
  bool success = toolchain.runCmdSyncLogged(inner);

  fs::path elfPath = buildDcDir / (project.conf.romName + "_dc.elf");
  if (success && !fs::exists(elfPath)) {
    Utils::Logger::log(
      "Dreamcast make returned success but ELF is missing: " + elfPath.generic_string(),
      Utils::Logger::LEVEL_ERROR);
    success = false;
  }

  if (success) {
    Utils::Logger::log("Dreamcast build done! Output: " + elfPath.generic_string());
    if (fs::exists(buildDcDir / (project.conf.romName + "_dc.cdi")))
      Utils::Logger::log("CDI: " + (buildDcDir / (project.conf.romName + "_dc.cdi")).generic_string());
    else
      Utils::Logger::log("ELF built; install mkdcdisc for CDI packaging.", Utils::Logger::LEVEL_WARN);
  } else {
    Utils::Logger::log("Dreamcast build failed!", Utils::Logger::LEVEL_ERROR);
  }
  return success;
}

bool Build::cleanProject(const Project::Project &project, const CleanArgs &args)
{
  Utils::Logger::log("Clean Project: " + project.getPath());
  fs::path projPath{project.getPath()};

  fs::remove(projPath / (project.conf.romName + ".z64"));

  if(args.assets) {
    fs::remove_all(projPath / "filesystem");
  }
  if(args.code) {
    fs::remove_all(projPath / "build");
    fs::remove_all(projPath / "build-pc");
    fs::remove_all(projPath / "build-dc");
  }
  if(args.engine) {
    fs::remove_all(projPath / "engine" / "build");
  }
  if(args.engineSrc) {
    fs::remove_all(projPath / "engine");
  }

  return true;
}


bool Build::assetBuildNeeded(const Project::AssetManagerEntry &asset, const fs::path &outPath)
{
  auto ageSrc = Utils::FS::getFileAge(asset.path);
  auto ageDst = Utils::FS::getFileAge(outPath);
  if(ageSrc < ageDst) {
    //Utils::Logger::log("Skipping Asset (up to date): " + asset.outPath);
    return false;
  }
  Utils::Logger::log("Building Asset: " + asset.path);
  return true;
}
