﻿

#pragma once
//------------------------------------------------------------------------------
//     This code was generated by a tool.
//
//     Changes to this file may cause incorrect behavior and will be lost if
//     the code is regenerated.
//------------------------------------------------------------------------------

namespace Windows {
    namespace UI {
        namespace Xaml {
            namespace Controls {
                ref class StackPanel;
                ref class TextBlock;
                ref class Canvas;
                ref class Image;
            }
        }
    }
}
namespace HDFaceVisualizer {
    ref class HDFaceD3DPanel;
}

namespace Microsoft
{
    namespace Samples
    {
        namespace Kinect
        {
            namespace HDFaceBasics
            {
                partial ref class MainPage : public ::Windows::UI::Xaml::Controls::Page, 
                    public ::Windows::UI::Xaml::Markup::IComponentConnector
                {
                public:
                    void InitializeComponent();
                    virtual void Connect(int connectionId, ::Platform::Object^ target);
                
                private:
                    bool _contentLoaded;
                
                    private: ::Windows::UI::Xaml::Controls::StackPanel^ statusBar;
                    private: ::Windows::UI::Xaml::Controls::TextBlock^ lblSkinColor;
                    private: ::Windows::UI::Xaml::Controls::Canvas^ skinColor;
                    private: ::Windows::UI::Xaml::Controls::TextBlock^ lblHairColor;
                    private: ::Windows::UI::Xaml::Controls::Canvas^ hairColor;
                    private: ::Windows::UI::Xaml::Controls::Image^ imgKinectView;
                    private: ::HDFaceVisualizer::HDFaceD3DPanel^ HDFaceRenderingPanel;
                    private: ::Windows::UI::Xaml::Controls::TextBlock^ lblFaceDeforms;
                    private: ::Windows::UI::Xaml::Controls::TextBlock^ lblAnimUnits;
                };
            }
        }
    }
}
