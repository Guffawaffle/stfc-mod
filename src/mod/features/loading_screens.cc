// The mod's loading screens, from [graphics] loader_*: the login background and logos, the transition screen between
// sections, and the loading tips. They share the embedded images and reset together when the game reloads.

#include "community_badge.h"
#include "config.h"
#include "features/features.h"
#include "file.h"
#include "frame.h"

#include "game/loading.h"
#include "game/ui.h"

#include <il2cpp/hook.h>
#include <il2cpp/il2cpp.h>
#include <spdlog/spdlog.h>
#include <unity/ui.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <random>
#include <string_view>
#include <utility>
#include <vector>

// Generated into the Zig build cache from assets/ (or -Dbg_image).
#include "embedded_cc_logo_image.h"
#include "embedded_loading_image.h"

namespace
{
bool ShowSprite(unity::Image* image, unity::Sprite* sprite, bool preserve_aspect)
{
  return image->TryCall<"set_sprite">(sprite) && image->TryCall<"set_color">(unity::Color{1.0f, 1.0f, 1.0f, 1.0f})
         && image->TryCall<"set_type">(unity::Image::Type::Simple)
         && image->TryCall<"set_preserveAspect">(preserve_aspect) && image->TryCall<"set_raycastTarget">(false);
}

void HideImage(unity::Image* image)
{
  if (image)
    image->SetColor({0.0f, 0.0f, 0.0f, 0.0f});
}

// Decodes `data` into `asset` unless it is still loaded; warns on every failed decode.
unity::ImageAsset* LoadScreenImage(unity::ImageAsset& asset, const uint8_t* data, size_t size, const char* name)
{
  if (asset.Load(data, size))
    return &asset;
  spdlog::warn("[LoadingScreens] could not load the {} image", name);
  return nullptr;
}

struct ScreenImages {
  unity::ImageAsset background, community_logo;

  // The configured loader_image, or the embedded one when it is unset or unreadable. Called from hooks, so it
  // never throws.
  unity::ImageAsset* Background()
  {
    if (background.Texture())
      return &background;
    // loader_image is UTF-8; File::FromUtf8 keeps Windows from reading it in the ANSI code page.
    const auto      path = File::FromUtf8(Config::Get().graphics.loader_image);
    std::error_code error;
    if (!path.empty() && std::filesystem::exists(path, error)) {
      std::ifstream        file(path, std::ios::binary);
      std::vector<uint8_t> data(std::istreambuf_iterator<char>(file), {});
      if (!data.empty() && LoadScreenImage(background, data.data(), data.size(), "custom loading background"))
        return &background;
    }
#ifndef _USE_ORIGINAL_BG
    return LoadScreenImage(background, g_embeddedLoadingImage, g_embeddedLoadingImage_SIZE,
                           "embedded loading background");
#else
    return nullptr;
#endif
  }

  unity::ImageAsset* CommunityLogo()
  { return LoadScreenImage(community_logo, g_embeddedCCLogoImage, g_embeddedCCLogoImage_SIZE, "community logo"); }
};
// Leaked on purpose: its GC handles must not be released by static destructors after the runtime is gone.
ScreenImages& screen_images = *new ScreenImages;

unity::GameObject* CreateOverlay(const char* name, unity::ImageAsset& asset, unity::Transform* parent,
                                 unity::Vector2 anchor_min, unity::Vector2 anchor_max, bool preserve_aspect,
                                 bool behind_siblings)
{
  auto* sprite = parent ? asset.Sprite() : nullptr;
  auto* object = sprite ? il2cpp::Allocate<unity::GameObject>() : nullptr;
  if (!object)
    return nullptr;
  // A failed attempt can already own a Unity object parented to the canvas.
  // Releasing its managed root alone would leave an orphan overlay behind.
  struct ProvisionalLoadingOverlay {
    il2cpp::GcHandle root;
    bool             constructed = false, published = false;
    ~ProvisionalLoadingOverlay()
    {
      if (constructed && !published)
        unity::Destroy(root.Get());
    }
  } provisional{il2cpp::GcHandle::Strong(object)};
  if (!provisional.root)
    return nullptr;
  // New<GameObject> contains constructor exceptions but returns the allocation.
  // Here successful construction is required before an overlay can be published.
  if (!object->TryCall<".ctor">(il2cpp::NewString(name)))
    return nullptr;
  provisional.constructed = true;
  auto* transform         = object->GetComponent<unity::Transform>();
  if (!transform)
    return nullptr;
  // Attached to the canvas root, so the overlay covers the whole screen.
  auto* canvas = unity::CanvasRoot(parent);
  if (!canvas || !transform->TryCall<"SetParent">(canvas, false))
    return nullptr;
  if (behind_siblings)
    transform->SetAsFirstSibling();

  // Adding a Graphic turns the plain Transform into a RectTransform.
  auto* image = object->AddComponent<unity::Image>();
  auto* rect  = image ? object->GetComponent<unity::RectTransform>() : nullptr;
  if (!rect)
    return nullptr;
  if (!rect->TryCall<"set_anchorMin">(anchor_min) || !rect->TryCall<"set_anchorMax">(anchor_max)
      || !rect->TryCall<"set_pivot">(unity::Vector2{0.5f, 0.5f})
      || !rect->TryCall<"set_sizeDelta">(unity::Vector2{0.0f, 0.0f})
      || !rect->TryCall<"set_anchoredPosition">(unity::Vector2{0.0f, 0.0f})
      || !ShowSprite(image, sprite, preserve_aspect))
    return nullptr;
  provisional.published = true;
  return object;
}

// The bottom-left badge is laid out in pixels for a 1920-wide screen, then scaled to the real width and by
// loader_logo_scale. The overlay is placed purely by anchors, so the pixel box becomes fractions of the screen.
bool CreateLogo(const char* name, unity::ImageAsset* asset, unity::Transform* parent)
{
  if (!asset || !parent)
    return false;

  // Screen can report 0 early in startup; fall back to a phone-sized screen.
  const auto  screen_width = unity::Screen::width(), screen_height = unity::Screen::height();
  const float width  = screen_width > 0 ? static_cast<float>(screen_width) : 1334.0f;
  const float height = screen_height > 0 ? static_cast<float>(screen_height) : 750.0f;

  const float scale       = width / 1920.0f * Config::Get().graphics.loader_logo_scale;
  const float logo_width  = 150.0f * scale;
  const float logo_height = logo_width * asset->AspectRatio();
  const float pad_x       = 30.0f * scale;
  const float pad_y       = 185.0f * scale;

  return CreateOverlay(name, *asset, parent, {pad_x / width, pad_y / height},
                       {(pad_x + logo_width) / width, (pad_y + logo_height) / height}, true, false);
}

// One app badge, shared with the launcher, without a duplicate mod emblem on the right.
struct ScreenLogos {
  bool community_logo = false;

  // Creates the badge once for this scene.
  void Create(unity::Transform* parent)
  { community_logo = community_logo || CreateLogo("STFCCCLogo", screen_images.CommunityLogo(), parent); }
};

// --- Login sequence ---

void ShowLoginWelcome(LoginSequence* sequence);
bool login_artwork_installed = false, login_welcome_installed = false;

// The login screen's background image: the first child Image with a known background name, or else the first
// Image found at all, searching four levels deep.
unity::Image* FindLoginBackground(unity::Transform* transform, int depth, unity::Image*& first)
{
  static constexpr std::string_view kNames[] = {"Background", "BG",       "bg_image", "LoadingBackground",
                                                "LoginBG",    "SplashBG", "Image"};
  if (!transform || depth > 4)
    return nullptr;
  for (auto* child : transform->Children()) {
    auto* object = child ? child->gameObject() : nullptr;
    if (!object)
      continue;
    if (auto* image = object->GetComponent<unity::Image>()) {
      if (!first)
        first = image;
      if (std::ranges::contains(kNames, object->Name()))
        return image;
    }
    if (auto* found = FindLoginBackground(child, depth + 1, first))
      return found;
  }
  return nullptr;
}

void LoginSequence_Awake_Hook(auto original, LoginSequence* _this)
{
  original(_this);
  ShowLoginWelcome(_this);
  if (!login_artwork_installed || !Config::Get().graphics.loader_enabled)
    return;

  // The next login generation must observe changed files and discard the old
  // rooted wrappers even if Unity has not unloaded their native textures yet.
  screen_images = {};
  auto* canvas  = _this->mainCanvas();
  auto* root    = canvas ? canvas->transform() : nullptr;
  if (!root)
    return;

  unity::Image* first = nullptr;
  auto*         found = FindLoginBackground(root, 0, first);
  auto*         image = found ? found : first;
  if (!image)
    return;

  auto* background = screen_images.Background();
  if (auto* sprite = background ? background->Sprite() : nullptr) {
    image->SetOverrideSprite(sprite);
    ShowSprite(image, sprite, false);
    image->SetVerticesDirty();
  }
  ScreenLogos{}.Create(image->transform());
}

// --- Loading tips ---

struct LoadingTip {
  const char* text;
  int         weight;
};

constexpr char kLoadingWelcome[] =
    "You're using Guffawaffle's fork of the STFC Community Mod. Report issues or request features at "
    "github.com/Guffawaffle/stfc-mod/issues";

constexpr LoadingTip kTipTexts[] = {
    {"Did you know? You can customize zoom presets, hotkeys, and UI scale in the community mod settings file to suit "
     "your playstyle.",
     15},
    {"The community mod supports custom loading screens, transition backgrounds, and borderless fullscreen mode for a "
     "better experience.",
     10},
    {"Press F11 at any time to toggle between borderless fullscreen and windowed mode without restarting the game or "
     "losing your session.",
     15},
    {"Visit https://stfc.pro to track player rankings and stats across over 236,000 players and 114 servers in Star "
     "Trek Fleet Command.",
     50},
    {"Join the community Discord server to report bugs, request new features, and stay up to date with the latest mod "
     "releases and updates.",
     10},
};

// The first loading screen of a session gets the welcome line. After that, half the transition screens keep the
// game's own tip and the rest get one of ours by weight, never the same one twice in a row.
struct TipPicker {
  bool         first_screen = true;
  size_t       last         = std::size(kTipTexts);
  std::mt19937 random{std::random_device{}()};

  const char* Next()
  {
    if (std::exchange(first_screen, false))
      return kLoadingWelcome;
    if (std::bernoulli_distribution(0.5)(random))
      return nullptr;

    std::array<int, std::size(kTipTexts)> weights{};
    for (size_t i = 0; i < weights.size(); ++i)
      weights[i] = i == last ? 0 : kTipTexts[i].weight;
    last = std::discrete_distribution<size_t>(weights.begin(), weights.end())(random);
    return kTipTexts[last].text;
  }

  void Reset()
  {
    first_screen = true;
    last         = std::size(kTipTexts);
  }
} tip_picker;

void ShowLoginWelcome(LoginSequence* sequence)
{
  if (!login_welcome_installed || !Config::Get().graphics.loader_tip_enabled)
    return;
  auto*          canvas = sequence->mainCanvas();
  auto*          parent = canvas ? canvas->transform() : nullptr;
  auto*          owner  = parent ? parent->gameObject() : nullptr;
  constexpr auto name   = "STFCModWelcome";
  if (!owner || parent->Find(name))
    return;
  // Reuse the login screen's native font, including donors hidden at this login stage.
  const auto       observed   = owner->TryComponentsInChildren(unity::TMP_Text::Class::TypeObject(), true);
  auto             components = il2cpp::GcHandle::Strong(observed ? observed.value : nullptr);
  const auto       checked    = il2cpp::TryReferenceArray<unity::Component*>(components.Get());
  il2cpp::GcHandle font;
  if (checked && checked.value)
    for (auto* component : *checked.value) {
      auto  donor = il2cpp::GcHandle::Strong(component ? component->As<unity::TMP_Text>() : nullptr);
      auto* text  = donor.Get<unity::TMP_Text>();
      font        = il2cpp::GcHandle::Strong(text ? text->font() : nullptr);
      if (font && unity::Alive(font.Get()))
        break;
      font.Reset();
    }
  if (!font)
    return;
  struct ProvisionalWelcome {
    il2cpp::GcHandle object;
    bool             published = false;
    ~ProvisionalWelcome()
    {
      if (!published && unity::Alive(object.Get()))
        unity::Destroy(object.Get());
    }
  } provisional;
  auto* rect   = unity::CreateUiObject(name, parent, provisional.object);
  auto* object = provisional.object.Get<unity::GameObject>();
  auto* label  = rect && object ? object->AddComponent<unity::TextMeshProUGUI>() : nullptr;
  if (!label || rect->parent() != parent)
    return;
  rect->Place({0.12f, 0.22f}, {0.88f, 0.30f}, {0.5f, 0.5f}, {0, 0}, {0, 0});
  if (!label->TryCall<"set_font">(font.Get<il2cpp::Object>()) || !label->TryCall<"set_fontSize">(20.0f)
      || !label->TryCall<"set_alignment">(unity::TMP_Text::Alignment::Center)
      || !label->TryCall<"set_color">(unity::Color{1, 1, 1, 1}) || !label->TryCall<"set_raycastTarget">(false)
      || !label->TryCall<"set_enableWordWrapping">(true)
      || !label->TryCall<"set_text">(il2cpp::NewString(kLoadingWelcome)) || !object->TryCall<"SetActive">(true))
    return;
  provisional.published   = true;
  tip_picker.first_screen = false;
}

void LoadingTipViewController_SetRandomTipLocalisedText_Hook(auto original, LoadingTipViewController* _this)
{
  original(_this);
  if (!Config::Get().graphics.loader_tip_enabled)
    return;
  auto* label = _this->textMeshPro();
  if (label)
    if (const char* tip = tip_picker.Next())
      label->SetText(tip);
}

// --- Transition screen ---

// What has been done to the current TransitionViewController. Its Unity objects are looked up again on each
// attempt rather than kept, since nothing roots them between hooks. A new controller or a reload starts over.
struct TransitionState {
  il2cpp::GcHandle owner, animator;
  bool             customized = false;
  bool             overlay    = false;
  ScreenLogos      logos;
};
// Like ScreenImages, weak handles must not be released after runtime shutdown.
TransitionState& transition_screen = *new TransitionState;

bool OwnTransition(TransitionViewController* controller, bool restart = false)
{
  if (restart || transition_screen.owner.Get() != controller) {
    transition_screen       = {};
    transition_screen.owner = il2cpp::GcHandle::Weak(controller);
  }
  return transition_screen.owner.Get() == controller;
}

void DisableTransitionAnimator(TransitionViewController* controller)
{
  if (transition_screen.animator)
    return;
  if (auto* animator = controller->animator()) {
    auto root = il2cpp::GcHandle::Weak(animator);
    if (root && animator->TryCall<"set_enabled">(false))
      transition_screen.animator = std::move(root);
  }
}

// The transition background: _staticOverride, or the Image on BGContainer's first child (whose transform is
// returned in `box` so it can be stretched).
unity::Image* FindTransitionBackground(TransitionViewController* controller, unity::Transform*& box)
{
  if (auto* image = controller->staticOverride())
    return image;
  auto* root = controller->transform();
  if (!root)
    return nullptr;
  for (auto* child : root->Children()) {
    auto* object = child ? child->gameObject() : nullptr;
    if (!object || object->Name() != "BGContainer")
      continue;
    box          = child->childCount() > 0 ? child->GetChild(0) : nullptr;
    auto* inside = box ? box->gameObject() : nullptr;
    return inside ? inside->GetComponent<unity::Image>() : nullptr;
  }
  return nullptr;
}

// Moves the game logo to the top-right corner and the tip text below centre, out of the way of the background.
void ArrangeTransitionLayout(TransitionViewController* controller)
{
  auto* root = controller->transform();
  if (!root)
    return;
  for (auto* child : root->Children()) {
    auto* object = child ? child->gameObject() : nullptr;
    if (!object)
      continue;
    const auto name = object->Name();
    auto*      rect = object->GetComponent<unity::RectTransform>();
    if (rect && name == "LogoContainer")
      rect->Place({1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 1.0f}, {586.0f, 248.0f}, {-20.0f, -20.0f});
    else if (rect && name == "LoadingTipsContainer")
      rect->Place({0.5f, 0.5f}, {0.5f, 0.5f}, {0.5f, 0.5f}, {1024.0f, 100.0f}, {0.0f, -320.0f});
  }
}

// Runs from Awake and again from AboutToShow until it succeeds.
void CustomizeTransition(TransitionViewController* controller)
{
  unity::Transform* box        = nullptr;
  auto*             background = FindTransitionBackground(controller, box);
  if (!background)
    return;
  auto* parent = box ? box : background->transform();

  if (!Config::Get().graphics.loader_transition_black) {
    auto* asset = screen_images.Background();
    if (!asset) {
      if (parent) {
        transition_screen.logos.Create(parent);
        transition_screen.customized = true;
      }
      return;
    }
    if (parent && !transition_screen.overlay)
      transition_screen.overlay =
          CreateOverlay("STFCModBG", *asset, parent, {0.0f, 0.0f}, {1.0f, 1.0f}, false, true) != nullptr;
    // Keep the game's background unless the replacement is fully attached, so a failure is visible but harmless.
    if (!transition_screen.overlay) {
      spdlog::warn("[TransitionScreen] the custom background is unavailable; keeping the game's");
      return;
    }
    // The canvas animation would fade a successful replacement. Preserve native
    // animation too when construction fails; AboutToHide restores our animator.
    DisableTransitionAnimator(controller);
    HideImage(background);
    if (box) {
      if (auto* rect = box->As<unity::RectTransform>())
        rect->Place({0.0f, 0.0f}, {1.0f, 1.0f}, {0.5f, 0.5f}, {0.0f, 0.0f}, {0.0f, 0.0f});
      box->SetLocalEulerAngles({0, 0, 0});
      box->SetLocalScale({1, 1, 1});
    }
  } else
    DisableTransitionAnimator(controller);

  if (parent)
    transition_screen.logos.Create(parent);
  if (!Config::Get().graphics.loader_transition_black)
    ArrangeTransitionLayout(controller);
  transition_screen.customized = true;
}

void TransitionViewController_Awake_Hook(auto original, TransitionViewController* _this)
{
  original(_this);
  if (!Config::Get().graphics.loader_transition)
    return;
  screen_images = {};
  if (OwnTransition(_this, true))
    CustomizeTransition(_this);
}

void TransitionViewController_AboutToShow_Hook(auto original, TransitionViewController* _this)
{
  original(_this);
  if (Config::Get().graphics.loader_transition && OwnTransition(_this) && !transition_screen.customized)
    CustomizeTransition(_this);
}

void TransitionViewController_AboutToHide_Hook(auto original, TransitionViewController* _this)
{
  if (Config::Get().graphics.loader_transition && transition_screen.owner.Get() == _this)
    if (auto* animator = transition_screen.animator.Get<unity::Behaviour>(); animator && unity::Alive(animator))
      animator->SetEnabled(true);
  original(_this);
}

void SlideShowViewController_ShowCurrentSlide_Hook(auto original, SlideShowViewController* _this)
{
  original(_this);
  const auto& config = Config::Get();
  if (config.graphics.loader_transition && !config.graphics.loader_transition_black && transition_screen.overlay)
    HideImage(_this->image());
}

// Teardown owns the native hierarchy. Drop rooted image wrappers and session observations without touching it.
void ResetLoadingScreens()
{
  transition_screen = {};
  screen_images     = {};
  tip_picker.Reset();
}

bool EnsureLoginSequenceHook()
{
  static const bool installed = [] {
    il2cpp::HookSet hooks("LoginScreen");
    IL2CPP_STAGE(hooks, LoginSequence::Class::Method("Awake"), LoginSequence_Awake_Hook);
    return hooks.Commit();
  }();
  return installed;
}

bool InstallLoginScreen()
{
  login_artwork_installed = EnsureReloadGuards() && EnsureLoginSequenceHook();
  return login_artwork_installed;
}

bool InstallTransitionScreen()
{
  if (!EnsureReloadGuards())
    return false;
  il2cpp::HookSet hooks("TransitionScreen");
  IL2CPP_STAGE(hooks, TransitionViewController::Class::Method("Awake"), TransitionViewController_Awake_Hook);
  IL2CPP_STAGE(hooks, TransitionViewController::Class::Method("AboutToShow"),
               TransitionViewController_AboutToShow_Hook);
  IL2CPP_STAGE(hooks, TransitionViewController::Class::Method("AboutToHide"),
               TransitionViewController_AboutToHide_Hook);
  IL2CPP_STAGE(hooks, SlideShowViewController::Class::Method("ShowCurrentSlide"),
               SlideShowViewController_ShowCurrentSlide_Hook);
  return hooks.Commit();
}

bool InstallLoadingTips()
{
  if (!EnsureReloadGuards())
    return false;
  il2cpp::HookSet hooks("LoadingTips");
  IL2CPP_STAGE(hooks, LoadingTipViewController::Class::Method("SetRandomTipLocalisedText"),
               LoadingTipViewController_SetRandomTipLocalisedText_Hook);
  const bool installed    = hooks.Commit();
  login_welcome_installed = installed && EnsureLoginSequenceHook();
  if (installed && !login_welcome_installed)
    spdlog::warn("[LoadingTips] login welcome unavailable; keeping the native tip callback");
  return installed;
}
} // namespace

unity::Sprite* CommunityBadgeSprite()
{
  auto* badge = screen_images.CommunityLogo();
  return badge ? badge->Sprite() : nullptr;
}

const Feature kLoginScreen{"LoginScreen", &Config::Patches::loadingscreenhooks, InstallLoginScreen,
                           nullptr,       InstallRequirement::kOptional,        ResetLoadingScreens};
const Feature kTransitionScreen{"TransitionScreen",
                                &Config::Patches::transitionscreenhooks,
                                InstallTransitionScreen,
                                nullptr,
                                InstallRequirement::kOptional,
                                ResetLoadingScreens};
const Feature kLoadingTips{"LoadingTips", &Config::Patches::loadingtiphooks, InstallLoadingTips,
                           nullptr,       InstallRequirement::kOptional,     ResetLoadingScreens};
