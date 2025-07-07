#pragma once

#include <opencv2/opencv.hpp>
#include <string>

std::string detectarCorPredominante(const cv::Mat& bgr_roi);
std::string classificarMarcaPorCor(const std::string& cor);