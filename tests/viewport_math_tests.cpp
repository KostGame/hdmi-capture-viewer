#include "viewport_math.hpp"
#include <cassert>
#include <cmath>

int main() {
    using namespace hcv;
    auto pixel=calculate_view(ViewMode::Pixel100,800,600,1920,1080,500,900);
    assert(pixel.destination.width==800 && pixel.destination.height==600);
    assert(pixel.source.x==500 && pixel.source.y==480); // vertical pan clamps at 1080-600
    pixel=calculate_view(ViewMode::Pixel100,2200,1200,1920,1080,0,0);
    assert(pixel.destination.x==140 && pixel.destination.y==60 && pixel.source.x==0);
    auto autoSmall=calculate_view(ViewMode::Auto,1600,1000,1920,1080);
    assert(std::abs(autoSmall.destination.width-1600)<.01f && std::abs(autoSmall.destination.height-900)<.01f);
    assert(autoSmall.source.width==1920 && autoSmall.source.height==1080);
    auto autoLarge=calculate_view(ViewMode::Auto,2200,1200,1920,1080);
    assert(autoLarge.destination.x==140 && autoLarge.destination.y==60);
    assert(autoLarge.destination.width==1920 && autoLarge.destination.height==1080);
    auto fit=calculate_view(ViewMode::Fit,1600,1000,1920,1080);
    assert(std::abs(fit.destination.width-1600)<.01f && std::abs(fit.destination.height-900)<.01f);
    auto fill=calculate_view(ViewMode::Fill,1600,1000,1920,1080);
    assert(fill.destination.width==1600 && fill.destination.height==1000);
    assert(fill.source.width<1920 && fill.source.height==1080);
    auto thumb=scrollbar_thumb(600,1080,240,592);
    assert(thumb.needed && thumb.length>0 && thumb.start>0 && thumb.start+thumb.length<=592);
    assert(!scrollbar_thumb(1080,1080,0,100).needed);
    assert(borderless_hit_test(0,0,800,600,8)==ResizeEdge::TopLeft);
    assert(borderless_hit_test(400,4,800,600,8)==ResizeEdge::Top);
    assert(borderless_hit_test(400,12,800,600,8)==ResizeEdge::None);
    assert(borderless_hit_test(400,40,800,600,8)==ResizeEdge::None);
    for (int y=8; y<592; ++y) for (int x=8; x<792; ++x)
        assert(borderless_hit_test(x,y,800,600,8)==ResizeEdge::None);
}
