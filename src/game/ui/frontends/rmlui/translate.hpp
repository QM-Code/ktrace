#pragma once

#include <RmlUi/Core/Element.h>

#include "karma/common/i18n/i18n.hpp"

namespace ui::rmlui {

void ApplyTranslations(Rml::Element *root, const karma::common::i18n::I18n &i18n);

} // namespace ui::rmlui
