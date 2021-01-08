#include <insitu_plugins/text_filter.hpp>

namespace insitu_plugins {

TextFilter::TextFilter(void)
{
    text = "Hello World!";
}

void TextFilter::setText(std::string text_)
{
    text = text_;
}

cv::Mat TextFilter::apply(cv::Mat img)
{
    NODELET_DEBUG("str: %s\n", text.c_str());
    cv::putText(
        img,
        text,
        cv::Point(50,50),
        cv::FONT_HERSHEY_DUPLEX,
        1,
        cv::Scalar(0,255,0),
        2,
        false
    );
    return img;
}

} // namespace insitu_plugins

PLUGINLIB_EXPORT_CLASS(insitu_plugins::TextFilter, insitu::Filter);