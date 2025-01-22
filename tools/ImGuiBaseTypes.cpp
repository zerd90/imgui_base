
#include <stdint.h>
#include "ImGuiBaseTypes.h"

#define DEFINE_FLAG_OPERATIONS(ENUM)                      \
    ENUM operator~(ENUM flag)                             \
    {                                                     \
        return (ENUM) ~(uint64_t)flag;                    \
    }                                                     \
    ENUM operator&(ENUM flag1, ENUM flag2)                \
    {                                                     \
        return (ENUM)((uint64_t)flag1 & (uint64_t)flag2); \
    }                                                     \
    ENUM operator|(ENUM flag1, ENUM flag2)                \
    {                                                     \
        return (ENUM)((uint64_t)flag1 | (uint64_t)flag2); \
    }                                                     \
    ENUM &operator&=(ENUM &flag1, ENUM flag2)             \
    {                                                     \
        flag1 = flag1 & flag2;                            \
        return flag1;                                     \
    }                                                     \
    ENUM &operator|=(ENUM &flag1, ENUM flag2)             \
    {                                                     \
        flag1 = flag1 | flag2;                            \
        return flag1;                                     \
    }

DEFINE_FLAG_OPERATIONS(IM_DRAW_FLAGS)
DEFINE_FLAG_OPERATIONS(IM_DRAW_LIST_FLAGS)
DEFINE_FLAG_OPERATIONS(IM_FONT_ATLAS_FLAGS)
DEFINE_FLAG_OPERATIONS(IMGUI_BACKEND_FLAGS)
DEFINE_FLAG_OPERATIONS(IMGUI_BUTTON_FLAGS)
DEFINE_FLAG_OPERATIONS(IMGUI_CHILD_FLAGS)
DEFINE_FLAG_OPERATIONS(IMGUI_COLOR_EDIT_FLAGS)
DEFINE_FLAG_OPERATIONS(IMGUI_CONFIG_FLAGS)
DEFINE_FLAG_OPERATIONS(IMGUI_COMBO_FLAGS)
DEFINE_FLAG_OPERATIONS(IMGUI_DOCK_NODE_FLAGS)
DEFINE_FLAG_OPERATIONS(IMGUI_DRAG_DROP_FLAGS)
DEFINE_FLAG_OPERATIONS(IMGUI_FOCUSED_FLAGS)
DEFINE_FLAG_OPERATIONS(IMGUI_HOVERED_FLAGS)
DEFINE_FLAG_OPERATIONS(IMGUI_INPUT_FLAGS)
DEFINE_FLAG_OPERATIONS(IMGUI_INPUT_TEXT_FLAGS)
DEFINE_FLAG_OPERATIONS(IMGUI_ITEM_FLAGS)
DEFINE_FLAG_OPERATIONS(IMGUI_POPUP_FLAGS)
DEFINE_FLAG_OPERATIONS(IMGUI_MULTISELECT_FLAGS)
DEFINE_FLAG_OPERATIONS(IMGUI_SELECTABLE_FLAGS)
DEFINE_FLAG_OPERATIONS(IMGUI_SLIDER_FLAGS)
DEFINE_FLAG_OPERATIONS(IMGUI_TAB_BAR_FLAGS)
DEFINE_FLAG_OPERATIONS(IMGUI_TAB_ITEM_FLAGS)
DEFINE_FLAG_OPERATIONS(IMGUI_TABLE_FLAGS)
DEFINE_FLAG_OPERATIONS(IMGUI_TABLE_COLUMN_FLAGS)
DEFINE_FLAG_OPERATIONS(IMGUI_TABLE_ROW_FLAGS)
DEFINE_FLAG_OPERATIONS(IMGUI_TREE_NODE_FLAGS)
DEFINE_FLAG_OPERATIONS(IMGUI_VIEWPORT_FLAGS)
DEFINE_FLAG_OPERATIONS(IMGUI_WINDOW_FLAGS);

ResourceGuard::ResourceGuard(std::function<void()> func) : releaseFunction(func) {}

ResourceGuard::~ResourceGuard()
{
    if (releaseFunction)
        releaseFunction();
    }
