// mm/2s2h/Rando/MiscBehavior/AdultLinkModel.cpp
// ComboShip (Cross-Games Age): a player-installed model for adult Link in Majora's Mask. See AdultLinkModel.h.
#ifdef COMBO_BUILD

#include "AdultLinkModel.h"
#include "2s2h/BenPort.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <tinyxml2.h>
#include <fast/resource/ResourceType.h>
#include <fast/resource/type/DisplayList.h>
#include <fast/resource/factory/DisplayListFactory.h>
#include <fast/resource/factory/MatrixFactory.h>
#include <fast/resource/factory/TextureFactory.h>
#include <fast/resource/factory/VertexFactory.h>
#include <ship/Context.h>
#include <ship/resource/CrossRMRegistry.h>
#include <ship/resource/File.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/Archive.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <ship/resource/factory/BlobFactory.h>
#include <spdlog/spdlog.h>

extern "C" {
#include "z64player.h"
}

namespace Combo::AdultLinkModel {

namespace {

constexpr char kRoute[] = "mmadult"; // the "@mmadult:" route (CrossRMRegistry)
constexpr char kFolder[] = "mods/mm-adult-link";
constexpr char kOotPrefix[] = "__OTR__@oot:";
constexpr char kModPrefix[] = "__OTR__@mmadult:";

bool sInitDone = false;
std::shared_ptr<Ship::ResourceManager> sRM;

// A limb list copied between a G_COMBO_RM_PUSH/POP bracket: the skeleton draw submits limb lists as raw pointers, so
// the bracket is what sends the paths inside (materials, vertices, OOT's textures) to this ResourceManager.
struct BuiltLimb {
    std::shared_ptr<Ship::IResource> res; // keeps the mod's list (and the path strings its commands point at) alive
    std::vector<Gfx> gfx;
};
BuiltLimb sLimbs[PLAYER_LIMB_MAX];
std::unordered_map<const char*, std::unique_ptr<char[]>> sPaths; // OOT path -> its routed twin, or nullptr
std::unordered_map<std::string, std::unique_ptr<char[]>> sEquip; // custom equipment name -> routed path, or nullptr

// Commands that take two Gfx words; an XML vertex load (G_VTX_OTR_FILEPATH) is one too.
bool IsTwoWordOpcode(uint8_t op) {
    return (op == G_SETTIMG_OTR_HASH) || (op == G_DL_OTR_HASH) || (op == G_VTX_OTR_HASH) || (op == G_BRANCH_Z_OTR) ||
           (op == G_MARKER) || (op == G_MTX_OTR) || (op == G_MOVEMEM_OTR) || (op == G_VTX_OTR_FILEPATH);
}

std::string Strip(const char* path) {
    return (strncmp(path, "__OTR__", 7) == 0) ? std::string(path + 7) : std::string(path);
}

bool HasAlt(const std::string& path) {
    return sRM->GetArchiveManager()->HasFile(Ship::IResource::gAltAssetPrefix + path);
}

bool Exists(const std::string& path) {
    return HasAlt(path) || sRM->GetArchiveManager()->HasFile(path);
}

std::unique_ptr<char[]> Copy(const std::string& s) {
    auto out = std::make_unique<char[]>(s.size() + 1); // operator new[]: aligned, so never read as a segment address
    memcpy(out.get(), s.c_str(), s.size() + 1);
    return out;
}

// The mods' version of an XML resource, as text ("" when there's none).
std::string ReadXml(const std::string& path) {
    auto file = sRM->GetArchiveManager()->LoadFile(Ship::IResource::gAltAssetPrefix + path);
    if ((file == nullptr) || (file->Buffer == nullptr)) {
        return "";
    }
    return std::string(file->Buffer->data(), file->Buffer->size());
}

std::vector<std::string> ModFiles() {
    std::vector<std::string> files;
    try {
        std::string folder = Ship::Context::LocateFileAcrossAppDirs(kFolder, appShortName);
        if (!std::filesystem::exists(folder)) {
            // Made on first use, so players can find where the files go.
            folder = Ship::Context::GetPathRelativeToAppDirectory(kFolder, appShortName);
            if (std::filesystem::create_directories(folder)) {
                std::ofstream(folder + "/adult_link_model_mods_for_mm_go_here.txt").close();
            }
            return files;
        }
        for (const auto& entry : std::filesystem::directory_iterator(folder)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if ((ext == ".o2r") || (ext == ".otr") || (ext == ".zip")) {
                files.push_back(entry.path().string());
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        SPDLOG_ERROR("[ComboShip] adult Link model: can't read {}: {}", kFolder, e.what());
        files.clear();
    }
    // Where two mods replace the same thing, the file whose name sorts first wins: the last archive added wins.
    std::sort(files.begin(), files.end(), [](const std::string& a, const std::string& b) {
        return std::filesystem::path(a).filename().string() > std::filesystem::path(b).filename().string();
    });
    return files;
}

void RegisterFactories(const std::shared_ptr<Ship::ResourceLoader>& loader) {
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryTextureV0>(), RESOURCE_FORMAT_BINARY,
                                    "Texture", static_cast<uint32_t>(Fast::ResourceType::Texture), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryTextureV1>(), RESOURCE_FORMAT_BINARY,
                                    "Texture", static_cast<uint32_t>(Fast::ResourceType::Texture), 1);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryVertexV0>(), RESOURCE_FORMAT_BINARY,
                                    "Vertex", static_cast<uint32_t>(Fast::ResourceType::Vertex), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryXMLVertexV0>(), RESOURCE_FORMAT_XML, "Vertex",
                                    static_cast<uint32_t>(Fast::ResourceType::Vertex), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryDisplayListV0>(),
                                    RESOURCE_FORMAT_BINARY, "DisplayList",
                                    static_cast<uint32_t>(Fast::ResourceType::DisplayList), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryXMLDisplayListV0>(), RESOURCE_FORMAT_XML,
                                    "DisplayList", static_cast<uint32_t>(Fast::ResourceType::DisplayList), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryMatrixV0>(), RESOURCE_FORMAT_BINARY,
                                    "Matrix", static_cast<uint32_t>(Fast::ResourceType::Matrix), 0);
    loader->RegisterResourceFactory(std::make_shared<Ship::ResourceFactoryBinaryBlobV0>(), RESOURCE_FORMAT_BINARY,
                                    "Blob", static_cast<uint32_t>(Ship::ResourceType::Blob), 0);
}

// The limb list, bracketed; calls to lists the mods don't have become no-ops (a model mod can leave out materials of
// limbs Ship of Harkinian never draws from the skeleton, and a missing call would stop the interpreter).
bool BuildLimb(BuiltLimb& out, const std::string& path) {
    out.res = sRM->LoadResource(path);
    if ((out.res == nullptr) ||
        (out.res->GetInitData()->Type != static_cast<uint32_t>(Fast::ResourceType::DisplayList))) {
        SPDLOG_ERROR("[ComboShip] adult Link model: limb list missing: {}", path);
        out.res = nullptr;
        return false;
    }
    const std::vector<Gfx>& in = std::static_pointer_cast<Fast::DisplayList>(out.res)->Instructions;
    Gfx push = gsSPComboRMPush(kRoute);
    out.gfx.push_back(push);
    for (size_t i = 0; i < in.size();) {
        const uint8_t op = (uint8_t)(in[i].words.w0 >> 24);
        if (op == G_ENDDL) {
            break;
        }
        const size_t words = IsTwoWordOpcode(op) ? 2 : 1;
        if ((op == G_DL_OTR_FILEPATH) && !Exists(Strip((const char*)in[i].words.w1))) {
            SPDLOG_WARN("[ComboShip] adult Link model: {} calls {}, which no mod has; skipped", path,
                        (const char*)in[i].words.w1);
            Gfx noop = gsDPNoOp();
            out.gfx.push_back(noop);
        } else {
            out.gfx.insert(out.gfx.end(), in.begin() + i, in.begin() + std::min(i + words, in.size()));
        }
        i += words;
    }
    Gfx pop = gsSPComboRMPop();
    Gfx end = gsSPEndDisplayList();
    out.gfx.push_back(pop);
    out.gfx.push_back(end);
    return true;
}

// A replaced adult skeleton: its limbs' lists become MM's adult Link limbs. Returns how many.
int LoadBody() {
    const std::string skeleton = ReadXml("objects/object_link_boy/gLinkAdultSkel");
    if (skeleton.empty()) {
        return 0;
    }
    tinyxml2::XMLDocument doc;
    if (doc.Parse(skeleton.c_str(), skeleton.size()) != tinyxml2::XML_SUCCESS) {
        SPDLOG_ERROR("[ComboShip] adult Link model: unreadable gLinkAdultSkel");
        return 0;
    }
    tinyxml2::XMLElement* root = doc.FirstChildElement("Skeleton");
    if (root == nullptr) {
        return 0;
    }
    int count = 0;
    int skelLimb = 0;
    for (tinyxml2::XMLElement* e = root->FirstChildElement("SkeletonLimb"); e != nullptr;
         e = e->NextSiblingElement("SkeletonLimb"), skelLimb++) {
        // OOT's skeleton has no "none" limb: its limb i is MM's limb i + 1 (Fierce Deity's skeleton is OOT adult's).
        const int limb = skelLimb + 1;
        if ((limb >= PLAYER_LIMB_MAX) || (limb == PLAYER_LIMB_LEFT_HAND) || (limb == PLAYER_LIMB_RIGHT_HAND) ||
            (limb == PLAYER_LIMB_SHEATH)) {
            continue; // OOT draws the hands and sheath from its own lists, never from the skeleton
        }
        const char* limbPath = e->Attribute("Path");
        if (limbPath == nullptr) {
            continue;
        }
        const std::string limbXml = ReadXml(Strip(limbPath));
        tinyxml2::XMLDocument limbDoc;
        if (limbXml.empty() || (limbDoc.Parse(limbXml.c_str(), limbXml.size()) != tinyxml2::XML_SUCCESS)) {
            continue;
        }
        tinyxml2::XMLElement* limbElem = limbDoc.FirstChildElement("SkeletonLimb");
        const char* dl = (limbElem != nullptr) ? limbElem->Attribute("DisplayList1") : nullptr;
        if ((dl == nullptr) || (dl[0] == '\0') || (Strip(dl) == "gEmptyDL")) {
            continue;
        }
        if (BuildLimb(sLimbs[limb], Strip(dl))) {
            count++;
        }
    }
    return count;
}

void Release() {
    for (auto& limb : sLimbs) {
        limb.gfx.clear();
        limb.res = nullptr;
    }
}

} // namespace

void Init() {
    if (sInitDone) {
        return;
    }
    auto oot = Ship::CrossRMRegistry::Get("oot");
    if (oot == nullptr) {
        return; // OOT's archives aren't up yet: try again next time
    }
    sInitDone = true;
    const std::vector<std::string> mods = ModFiles();
    if (mods.empty()) {
        return;
    }
    // OOT's archives first (the mods' lists use OOT adult Link's textures, and anything they don't replace is OOT's),
    // then the mods on top.
    std::vector<std::string> paths;
    for (const auto& archive : *oot->GetArchiveManager()->GetArchives()) {
        paths.push_back(archive->GetPath());
    }
    paths.insert(paths.end(), mods.begin(), mods.end());
    sRM = std::make_shared<Ship::ResourceManager>();
    sRM->Init(paths, {}, 4096); // more threads reserved than any machine has: a one-thread pool (loads are synchronous)
    if (!sRM->IsLoaded()) {
        sRM = nullptr;
        return;
    }
    RegisterFactories(sRM->GetResourceLoader());
    sRM->SetAltAssetsEnabled(true); // model mods replace resources through "alt/"
    Ship::CrossRMRegistry::Register(kRoute, sRM);
    const int limbs = LoadBody();
    SPDLOG_INFO("[ComboShip] adult Link model: {} mod file(s) in {}, {} body limb(s) replaced", mods.size(), kFolder,
                limbs);
}

bool Active() {
    return sRM != nullptr;
}

Gfx* Limb(int limbIndex) {
    if ((sRM == nullptr) || (limbIndex <= PLAYER_LIMB_NONE) || (limbIndex >= PLAYER_LIMB_MAX) ||
        sLimbs[limbIndex].gfx.empty()) {
        return nullptr;
    }
    return sLimbs[limbIndex].gfx.data();
}

const char* Path(const char* ootPath) {
    if ((sRM == nullptr) || (ootPath == nullptr) || (strncmp(ootPath, kOotPrefix, sizeof(kOotPrefix) - 1) != 0)) {
        return ootPath;
    }
    auto it = sPaths.find(ootPath);
    if (it == sPaths.end()) {
        const std::string path(ootPath + sizeof(kOotPrefix) - 1);
        it = sPaths.emplace(ootPath, HasAlt(path) ? Copy(kModPrefix + path) : nullptr).first;
    }
    return (it->second != nullptr) ? it->second.get() : ootPath;
}

const char* Equip(const char* name) {
    if ((sRM == nullptr) || (name == nullptr)) {
        return nullptr;
    }
    auto it = sEquip.find(name);
    if (it == sEquip.end()) {
        const std::string path = std::string("objects/object_custom_equip/") + name;
        it = sEquip.emplace(name, HasAlt(path) ? Copy(kModPrefix + path) : nullptr).first;
    }
    return it->second.get();
}

} // namespace Combo::AdultLinkModel

extern "C" void Combo_AdultLinkModel_Shutdown(void) {
    using namespace Combo::AdultLinkModel;
    Release();
    if (sRM != nullptr) {
        Ship::CrossRMRegistry::Unregister(kRoute);
        sRM = nullptr; // dies here, on the main thread (see CrossRMRegistry::Unregister)
    }
}

#endif // COMBO_BUILD
