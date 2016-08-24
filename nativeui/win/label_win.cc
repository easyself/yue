// Copyright 2016 Cheng Zhao. All rights reserved.
// Use of this source code is governed by the license that can be found in the
// LICENSE file.

#include "nativeui/label.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "nativeui/win/subwin_view.h"

namespace nu {

namespace {

class LabelView : public BaseView {
 public:
  LabelView() : BaseView(true) {}

  void SetText(const base::string16& text) {
    text_ = text;
  }

  base::string16 GetText() const {
    return text_;
  }

  void Draw(Gdiplus::Graphics* context, const gfx::Rect& dirty) override {
    if (!window())
      return;

    gfx::Point origin(GetWindowPixelOrigin());

    Gdiplus::FontFamily family(L"Times New Roman");
    Gdiplus::Font font(&family, 24, Gdiplus::FontStyleRegular,
                       Gdiplus::UnitPixel);
    Gdiplus::PointF point(origin.x(), origin.y());
    Gdiplus::SolidBrush brush(Gdiplus::Color(255, 0, 0, 255));

    context->DrawString(text_.c_str(), static_cast<int>(text_.size()),
                        &font, point, &brush);
  }

 private:
  base::string16 text_;
};

}  // namespace

Label::Label(const std::string& text) {
  set_view(new LabelView());
  SetText(text);
}

Label::~Label() {
}

void Label::SetText(const std::string& text) {
  LabelView* label = static_cast<LabelView*>(view());
  label->SetText(base::UTF8ToUTF16(text));
}

std::string Label::GetText() {
  LabelView* label = static_cast<LabelView*>(view());
  return base::UTF16ToUTF8(label->GetText());
}

}  // namespace nu