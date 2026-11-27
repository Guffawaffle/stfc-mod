#pragma once

#include "loading_artwork_cases.h"

namespace
{
void LoadingTipsConsumerCases()
{
  auto& label_class = Define("TMPro", "TMP_Text", IL2CPP_TYPE_CLASS, &component_class);
  auto& tip_class = Define("Prime.LoadingScreen", "LoadingTipViewController", IL2CPP_TYPE_CLASS, &object_class);
  tip_class.klass.instance_size = sizeof(LoadingFixtureNode);
  AddField(tip_class, "_textMeshPro", label_class, offsetof(LoadingFixtureNode, image));
  auto& setter = AddMethod(label_class, "set_text", reinterpret_cast<void*>(&LoadingText), T(void_class),
                            {T(string_class)});
  AddProperty(label_class, "text", nullptr, &setter);
  auto* label = LoadingNode(&label_class.klass);
  auto* tip = LoadingNode(&tip_class.klass);
  tip->image = label;
  auto* view = reinterpret_cast<LoadingTipViewController*>(tip);
  auto& config = Config::Get().graphics;
  constexpr std::string_view welcome =
      "You're using Guffawaffle's fork of the STFC Community Mod. Report issues or request features at "
      "github.com/Guffawaffle/stfc-mod/issues";
  int originals = 0;
  auto original = [&](LoadingTipViewController*) { ++originals; label->text = "native tip"; };
  tip_picker.Reset();
  tip_picker.random.seed(417);
  config.loader_tip_enabled = false;
  LoadingTipViewController_SetRandomTipLocalisedText_Hook(original, view);
  Require(originals == 1 && label->text == "native tip" && tip_picker.first_screen,
          "disabled tips preserve native text without consuming the first welcome line");
  config.loader_tip_enabled = true;
  LoadingTipViewController_SetRandomTipLocalisedText_Hook(original, view);
  Require(originals == 2 && label->text == welcome,
          "the first enabled production tip writes the fork notice and support URL after native handling");
  std::string prior_custom;
  int custom = 0, native = 0;
  for (int index = 0; index < 200; ++index) {
    LoadingTipViewController_SetRandomTipLocalisedText_Hook(original, view);
    if (label->text == "native tip") {
      ++native;
    } else {
      Require(std::ranges::any_of(kTipTexts, [&](const auto& entry) { return label->text == entry.text; }),
              "transition tips use only the frozen source's native custom-tip catalog");
      Require(label->text != prior_custom, "native pass-through does not permit repeated consecutive custom tips");
      prior_custom = label->text;
      ++custom;
    }
  }
  Require(custom > 0 && native > 0 && originals == 202,
          "seeded production callback exercises both weighted custom text and native pass-through");
  ResetLoadingScreens();
  Require(tip_picker.first_screen && tip_picker.last == std::size(kTipTexts),
          "reload reset retires the previous session's tip selection");
  LoadingTipViewController_SetRandomTipLocalisedText_Hook(original, view);
  Require(label->text == welcome && originals == 203,
          "a reload makes the next enabled screen a fresh welcome");
  tip->image = nullptr;
  LoadingTipViewController_SetRandomTipLocalisedText_Hook(original, view);
  Require(originals == 204, "an unavailable native label preserves original delivery without dereferencing null");
  std::cout << "Loading tips production consumer passed\n";
}
} // namespace
