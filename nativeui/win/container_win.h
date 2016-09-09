// Copyright 2016 Cheng Zhao. All rights reserved.
// Use of this source code is governed by the license that can be found in the
// LICENSE file.

#ifndef NATIVEUI_WIN_CONTAINER_WIN_H_
#define NATIVEUI_WIN_CONTAINER_WIN_H_

#include "nativeui/container.h"
#include "nativeui/win/base_view.h"

namespace nu {

// The common base for views that have children.
class ContainerView : public BaseView {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void Layout() = 0;
    virtual std::vector<View*> GetChildren() = 0;
  };

  ContainerView(Delegate* delegate, ControlType type);

  // Baseview:
  void SizeAllocate(const Rect& size_allocation) override;
  void OnMouseMove(UINT flags, const Point& point) override;
  void OnMouseLeave() override;
  void OnMouseClick(UINT message, UINT flags, const Point& point) override;
  void Draw(PainterWin* painter, const Rect& dirty) override;
  void SetParent(BaseView* parent) override;
  void BecomeContentView(WindowImpl* parent) override;

 private:
  void RefreshParentTree();
  View* FindChildFromPoint(const Point& point);

  Delegate* delegate_;

  // The View in which mouse hovers.
  View* hover_view_ = nullptr;
};

}  // namespace nu

#endif  // NATIVEUI_WIN_CONTAINER_WIN_H_