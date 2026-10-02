#pragma once

#include <algorithm>
#include <cmath>

namespace hcv {

enum class ViewMode { Auto, Pixel100, Fit, Fill };
struct ViewRect { float x{}, y{}, width{}, height{}; };
struct ViewLayout { ViewRect destination, source; };
struct ScrollThumb { int start{}, length{}; bool needed{}; };

inline float clamp_pan(float pan, int source, int viewport) {
    return std::clamp(pan, 0.0f, static_cast<float>(std::max(0, source - viewport)));
}

inline ViewLayout calculate_view(ViewMode mode, int vw, int vh, int sw, int sh, float panX=0, float panY=0) {
    vw=std::max(1,vw); vh=std::max(1,vh); sw=std::max(1,sw); sh=std::max(1,sh);
    if (mode == ViewMode::Auto) {
        // PotPlayer-like default: preserve 1:1 source pixels when the whole
        // source fits, otherwise show the entire frame scaled down to fit.
        if (vw >= sw && vh >= sh) {
            const float dx=(vw-sw)*.5f, dy=(vh-sh)*.5f;
            return {{dx,dy,static_cast<float>(sw),static_cast<float>(sh)},
                {0,0,static_cast<float>(sw),static_cast<float>(sh)}};
        }
        const float scale=std::min(static_cast<float>(vw)/sw, static_cast<float>(vh)/sh);
        const float w=sw*scale, h=sh*scale;
        return {{(vw-w)*.5f,(vh-h)*.5f,w,h},{0,0,static_cast<float>(sw),static_cast<float>(sh)}};
    }
    if (mode == ViewMode::Pixel100) {
        const float cw=static_cast<float>(std::min(vw,sw)), ch=static_cast<float>(std::min(vh,sh));
        panX=clamp_pan(panX,sw,vw); panY=clamp_pan(panY,sh,vh);
        const float dx=sw<vw?(vw-sw)*.5f:0, dy=sh<vh?(vh-sh)*.5f:0;
        return {{dx,dy,cw,ch},{sw<vw?0.0f:panX,sh<vh?0.0f:panY,cw,ch}};
    }
    const float sx=static_cast<float>(vw)/sw, sy=static_cast<float>(vh)/sh;
    if (mode == ViewMode::Fit) {
        const float scale=std::min(sx,sy), w=sw*scale, h=sh*scale;
        return {{(vw-w)*.5f,(vh-h)*.5f,w,h},{0,0,static_cast<float>(sw),static_cast<float>(sh)}};
    }
    const float scale=std::max(sx,sy), visibleW=vw/scale, visibleH=vh/scale;
    return {{0,0,static_cast<float>(vw),static_cast<float>(vh)},
        {(sw-visibleW)*.5f,(sh-visibleH)*.5f,visibleW,visibleH}};
}

inline ScrollThumb scrollbar_thumb(int viewport, int source, float pan, int track, int minimum=18) {
    if (viewport<=0 || source<=viewport || track<=0) return {0,track,false};
    const int length=std::clamp(static_cast<int>(std::lround(static_cast<double>(track)*viewport/source)), std::min(minimum,track), track);
    const float maxPan=static_cast<float>(source-viewport);
    const int start=static_cast<int>(std::lround((track-length)*clamp_pan(pan,source,viewport)/maxPan));
    return {start,length,true};
}

enum class ResizeEdge { None, Left, Right, Top, Bottom, TopLeft, TopRight, BottomLeft, BottomRight };
inline ResizeEdge borderless_hit_test(int x,int y,int width,int height,int edge) {
    const bool l=x<edge, r=x>=width-edge, t=y<edge, b=y>=height-edge;
    if(t&&l) return ResizeEdge::TopLeft;
    if(t&&r) return ResizeEdge::TopRight;
    if(b&&l) return ResizeEdge::BottomLeft;
    if(b&&r) return ResizeEdge::BottomRight;
    if(l) return ResizeEdge::Left;
    if(r) return ResizeEdge::Right;
    if(t) return ResizeEdge::Top;
    if(b) return ResizeEdge::Bottom;
    return ResizeEdge::None;
}
}
