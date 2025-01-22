#ifndef _IMGUI_BASE_TYPES_H_
#define _IMGUI_BASE_TYPES_H_

#include <functional>
#include <mutex>

#include "imgui.h"

using StdMutex           = std::mutex;
using StdMutexGuard      = std::lock_guard<StdMutex>;
using StdMutexUniqueLock = std::unique_lock<StdMutex>;

using IMGUI_COLOR_STYLE     = enum ImGuiCol_;
using IMGUI_COND            = enum ImGuiCond_;
using IMGUI_DATATYPE        = enum ImGuiDataType_;
using IMGUI_MOUSE_BUTTON    = enum ImGuiMouseButton_;
using IMGUI_MOUSE_CURSOR    = enum ImGuiMouseCursor_;
using IMGUI_STYLE_VAR       = enum ImGuiStyleVar_;
using IMGUI_TABLE_BG_TARGET = enum ImGuiTableBgTarget_;

using IMGUI_KEYCHORD = ImGuiKeyChord;

using IM_DRAW_FLAGS            = enum ImDrawFlags_;
using IM_DRAW_LIST_FLAGS       = enum ImDrawListFlags_;
using IM_FONT_ATLAS_FLAGS      = enum ImFontAtlasFlags_;
using IMGUI_BACKEND_FLAGS      = enum ImGuiBackendFlags_;
using IMGUI_BUTTON_FLAGS       = enum ImGuiButtonFlags_;
using IMGUI_CHILD_FLAGS        = enum ImGuiChildFlags_;
using IMGUI_COLOR_EDIT_FLAGS   = enum ImGuiColorEditFlags_;
using IMGUI_CONFIG_FLAGS       = enum ImGuiConfigFlags_;
using IMGUI_COMBO_FLAGS        = enum ImGuiComboFlags_;
using IMGUI_DOCK_NODE_FLAGS    = enum ImGuiDockNodeFlags_;
using IMGUI_DRAG_DROP_FLAGS    = enum ImGuiDragDropFlags_;
using IMGUI_FOCUSED_FLAGS      = enum ImGuiFocusedFlags_;
using IMGUI_HOVERED_FLAGS      = enum ImGuiHoveredFlags_;
using IMGUI_INPUT_FLAGS        = enum ImGuiInputFlags_;
using IMGUI_INPUT_TEXT_FLAGS   = enum ImGuiInputTextFlags_;
using IMGUI_ITEM_FLAGS         = enum ImGuiItemFlags_;
using IMGUI_POPUP_FLAGS        = enum ImGuiPopupFlags_;
using IMGUI_MULTISELECT_FLAGS  = enum ImGuiMultiSelectFlags_;
using IMGUI_SELECTABLE_FLAGS   = enum ImGuiSelectableFlags_;
using IMGUI_SLIDER_FLAGS       = enum ImGuiSliderFlags_;
using IMGUI_TAB_BAR_FLAGS      = enum ImGuiTabBarFlags_;
using IMGUI_TAB_ITEM_FLAGS     = enum ImGuiTabItemFlags_;
using IMGUI_TABLE_FLAGS        = enum ImGuiTableFlags_;
using IMGUI_TABLE_COLUMN_FLAGS = enum ImGuiTableColumnFlags_;
using IMGUI_TABLE_ROW_FLAGS    = enum ImGuiTableRowFlags_;
using IMGUI_TREE_NODE_FLAGS    = enum ImGuiTreeNodeFlags_;
using IMGUI_VIEWPORT_FLAGS     = enum ImGuiViewportFlags_;
using IMGUI_WINDOW_FLAGS       = enum ImGuiWindowFlags_;

#define DECLARE_FLAG_OPERATIONS(ENUM)          \
    ENUM  operator~(ENUM flag);                \
    ENUM  operator&(ENUM flag1, ENUM flag2);   \
    ENUM  operator|(ENUM flag1, ENUM flag2);   \
    ENUM &operator&=(ENUM &flag1, ENUM flag2); \
    ENUM &operator|=(ENUM &flag1, ENUM flag2);

DECLARE_FLAG_OPERATIONS(IM_DRAW_FLAGS)
DECLARE_FLAG_OPERATIONS(IM_DRAW_LIST_FLAGS)
DECLARE_FLAG_OPERATIONS(IM_FONT_ATLAS_FLAGS)
DECLARE_FLAG_OPERATIONS(IMGUI_BACKEND_FLAGS)
DECLARE_FLAG_OPERATIONS(IMGUI_BUTTON_FLAGS)
DECLARE_FLAG_OPERATIONS(IMGUI_CHILD_FLAGS)
DECLARE_FLAG_OPERATIONS(IMGUI_COLOR_EDIT_FLAGS)
DECLARE_FLAG_OPERATIONS(IMGUI_CONFIG_FLAGS)
DECLARE_FLAG_OPERATIONS(IMGUI_COMBO_FLAGS)
DECLARE_FLAG_OPERATIONS(IMGUI_DOCK_NODE_FLAGS)
DECLARE_FLAG_OPERATIONS(IMGUI_DRAG_DROP_FLAGS)
DECLARE_FLAG_OPERATIONS(IMGUI_FOCUSED_FLAGS)
DECLARE_FLAG_OPERATIONS(IMGUI_HOVERED_FLAGS)
DECLARE_FLAG_OPERATIONS(IMGUI_INPUT_FLAGS)
DECLARE_FLAG_OPERATIONS(IMGUI_INPUT_TEXT_FLAGS)
DECLARE_FLAG_OPERATIONS(IMGUI_ITEM_FLAGS)
DECLARE_FLAG_OPERATIONS(IMGUI_POPUP_FLAGS)
DECLARE_FLAG_OPERATIONS(IMGUI_MULTISELECT_FLAGS)
DECLARE_FLAG_OPERATIONS(IMGUI_SELECTABLE_FLAGS)
DECLARE_FLAG_OPERATIONS(IMGUI_SLIDER_FLAGS)
DECLARE_FLAG_OPERATIONS(IMGUI_TAB_BAR_FLAGS)
DECLARE_FLAG_OPERATIONS(IMGUI_TAB_ITEM_FLAGS)
DECLARE_FLAG_OPERATIONS(IMGUI_TABLE_FLAGS)
DECLARE_FLAG_OPERATIONS(IMGUI_TABLE_COLUMN_FLAGS)
DECLARE_FLAG_OPERATIONS(IMGUI_TABLE_ROW_FLAGS)
DECLARE_FLAG_OPERATIONS(IMGUI_TREE_NODE_FLAGS)
DECLARE_FLAG_OPERATIONS(IMGUI_VIEWPORT_FLAGS)
DECLARE_FLAG_OPERATIONS(IMGUI_WINDOW_FLAGS);

#define DEFINE_FLAGS_VARIABLE_OPERARION(FLAG_TYPE, funcStr, variable) \
    void add##funcStr(FLAG_TYPE flag)                                 \
    {                                                                 \
        if (!(variable & flag))                                       \
            variable |= flag;                                         \
    }                                                                 \
    void remove##funcStr(FLAG_TYPE flag)                              \
    {                                                                 \
        if (variable & flag)                                          \
            variable &= (~flag);                                      \
    }                                                                 \
    void set##funcStr(FLAG_TYPE flags)                                \
    {                                                                 \
        if (variable != flags)                                        \
            variable = flags;                                         \
    }

class ResourceGuard
{
public:
    ResourceGuard(std::function<void()> func);
    virtual ~ResourceGuard();

private:
    std::function<void()> releaseFunction = nullptr;
};

#endif