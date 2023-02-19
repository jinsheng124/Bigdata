#include <cuda_provider_factory.h>
#include <onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>
#include<algorithm>
#include <numeric>
#include <iostream>
#include<fstream>
template<typename T>
// 排序索引
std::vector<int> sort_indexes(const std::vector<T>& v)
{
    std::vector<int> idx(v.size());
    std::iota(idx.begin(), idx.end(), 0);
    std::sort(idx.begin(), idx.end(), [&v](int i1, int i2) { return v[i1] > v[i2]; });
    return idx;
}
//iou计算
static float get_iou_value(std::vector<float>rect1, std::vector<float>rect2)
{
    float xx1, yy1, xx2, yy2;

    xx1 = std::max(rect1[0], rect2[0]);
    yy1 = std::max(rect1[1], rect2[1]);
    xx2 = std::min(rect1[2], rect2[2]);
    yy2 = std::min(rect1[3], rect2[3]);

    float insection_width, insection_height;
    insection_width = std::max(0.f, xx2 - xx1 + 1);
    insection_height = std::max(0.f, yy2 - yy1 + 1);

    float insection_area, union_area, iou;
    insection_area = insection_width * insection_height;
    union_area = (rect1[2]-rect1[0]) * (rect1[3] - rect1[1]) + (rect2[2] - rect2[0]) * (rect2[3] - rect2[1]) - insection_area;
    iou = insection_area / union_area;
    return iou;
}
//非极大抑制
void nms(std::vector<std::vector<float>> boxes, std::vector<float> confidences, float confThreshold, float nmsThreshold, std::vector<int>& indices){
    std::vector<int> idx = sort_indexes<float>(confidences);
    while (idx.size() > 0)
    {
        std::vector<int>ovr;
        int i = idx[0];
        indices.emplace_back(i);
        for (int j = 0; j < idx.size() - 1; j++)
        {
            auto iou = get_iou_value(boxes[i], boxes[idx[j + 1]]);
            if (iou < nmsThreshold) {
                ovr.emplace_back(idx[j + 1]);
            }
        }
        idx = ovr;
    }
}

cv::Mat detect(cv::Mat img,bool use_cuda = false)
{
    //const bool use_cuda = false;
    //const std::string fn_image = "person.jpg";

    std::vector<std::string>classnames;
    std::ifstream f("coco.names");
    std::string name = "";
    while (std::getline(f, name))
    {
        classnames.push_back(name);
    }

    // environment and options
    Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "OnnxModel");
    //Ort::Env env(OrtLoggingLevel::ORT_LOGGING_LEVEL_WARNING, "SuperResolution");
    Ort::SessionOptions session_options;
    //session_options.SetIntraOpNumThreads(1);
    if (use_cuda) {
        // https://github.com/microsoft/onnxruntime/blob/rel-1.6.0/include/onnxruntime/core/providers/cuda/cuda_provider_factory.h#L13
        OrtStatus* status = OrtSessionOptionsAppendExecutionProvider_CUDA(session_options, 0);
    }
    session_options.SetGraphOptimizationLevel(
        GraphOptimizationLevel::ORT_ENABLE_ALL);
    //自行将pt权重转为onnx模型
    const wchar_t* model_path = L"yolov5s.onnx";
    //#ifdef _WIN32
    //    const wchar_t* model_path = L"yolov5s_cuda.onnx";
    //#else
    //    const char* model_path = "yolov5s_cuda.onnx";
    //#endif

    // load model and create session
    Ort::Session session(env, model_path, session_options);
    Ort::AllocatorWithDefaultOptions allocator;

    // model info
    const char* input_name = session.GetInputName(0, allocator);
    const char* output_name = session.GetOutputName(0, allocator);
    auto input_dims = session.GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
    auto output_dims = session.GetOutputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
    input_dims[0] = output_dims[0] = 1;
    std::vector<const char*> input_names{ input_name };
    std::vector<const char*> output_names{ output_name };
    
    // input & output data
    cv::Mat image;
    cv::resize(img, image, cv::Size(640, 640), cv::INTER_AREA);
    cv::cvtColor(image, image, cv::COLOR_BGR2RGB);  // BGR -> RGB
    cv::Mat blob = cv::dnn::blobFromImage(image, 1.0 / 255.0);
    //cv::Mat output(output_dims[1], output_dims[2], CV_32FC1);
    auto memory_info = Ort::MemoryInfo::CreateCpu(
        OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);
    std::vector<Ort::Value> input_tensors;
    input_tensors.emplace_back(Ort::Value::CreateTensor<float>(
        memory_info, blob.ptr<float>(), blob.total(), input_dims.data(), input_dims.size()));
    auto output_tensors = session.Run(Ort::RunOptions{ nullptr }, input_names.data(), input_tensors.data(), input_names.size(), output_names.data(), output_names.size());
    float* floatarr = output_tensors[0].GetTensorMutableData<float>();
    float confThreshold = 0.4f;
    float nmsThreshold = 0.5f;
    std::vector<std::vector<float>>boxes;
    std::vector<std::vector<float>>nms_boxes;
    std::vector<float>confidences;
    std::vector<int>pre_cls;
    int max_size = 4096;
    for (int i = 0; i < output_dims[1]; i++) {
        int cur = i * output_dims[2];
        float conf = floatarr[cur + 4];
        if (conf <= confThreshold)continue;
        float max_conf = floatarr[cur + 5];
        int index = 5;
        for (int j = 5; j < output_dims[2]; j++)
        {
            if (floatarr[cur + j] > max_conf)
            {
                max_conf = floatarr[cur + j];
                index = j;
            }
        }
        conf *= max_conf;
        if (conf <= confThreshold)continue;
        index -= 5;
        int offset = index * max_size;
        std::vector<float>box;
        box.emplace_back(floatarr[cur] - floatarr[cur + 2] / 2.0);
        box.emplace_back(floatarr[cur + 1] - floatarr[cur + 3] / 2.0);
        box.emplace_back(floatarr[cur] + floatarr[cur + 2] / 2.0);
        box.emplace_back(floatarr[cur + 1] + floatarr[cur + 3] / 2.0);
        boxes.emplace_back(box);
        nms_boxes.emplace_back(std::vector<float>({ box[0] + offset, box[1] + offset, box[2] + offset, box[3] + offset }));
        confidences.emplace_back(conf);
        pre_cls.emplace_back(index);
        }
    //for (int i = 0; i < boxes.size(); i++) {
    //    std::cout << boxes[i][0] << " " << boxes[i][1] << " " << boxes[i][2] << " " << boxes[i][3] << " " << pre_cls[i] << std::endl;
    //}
    //std::cout << "------------------------------------" << std::endl;
    std::vector<int>indices;
    nms(nms_boxes,confidences,confThreshold,nmsThreshold,indices);
    for (int i = 0; i < indices.size(); i++) {
        float left = boxes[indices[i]][0] / 640 * img.cols;
        float top = boxes[indices[i]][1] / 640 * img.rows;
        float right = boxes[indices[i]][2] / 640 * img.cols;
        float bottom = boxes[indices[i]][3] / 640 * img.rows;
        cv::rectangle(img, cv::Rect(left, top, (right - left), (bottom - top)), cv::Scalar(0, 255, 0), 2);

        cv::putText(img,
            classnames[pre_cls[indices[i]]] + ":" + cv::format("%.2f", confidences[indices[i]]),
            cv::Point(left, top),
            cv::FONT_HERSHEY_SIMPLEX, 0.75, cv::Scalar(0, 255, 0), 2);

        //std::cout << boxes[indices[i]][0] <<" "<< boxes[indices[i]][1]<< " " << boxes[indices[i]][2]<< " " << boxes[indices[i]][3]<< " " << pre_cls[indices[i]]<<std::endl;
    }
    return img;

}
int main() {
    //std::string image_path = "1.jpg";
    //cv::Mat fn_image = cv::imread(image_path);
    //cv::Mat img = detect(fn_image, false);
    //cv::imshow("", img);
    //cv::waitKey(0);
    //cv::destroyAllWindows();
    cv::VideoCapture cap = cv::VideoCapture(0);
    cv::Mat frame;
    cap.read(frame);
    while (cap.isOpened())
    {
        cap.read(frame);
        if (frame.empty())
        {
            std::cout << "Read frame failed!" << std::endl;
            break;
        }
        frame = detect(frame, false);
        cv::imshow("detected(ESC退出)", frame);
        if (cv::waitKey(1) == 27) break;

    }
    cap.release();
    return 0;
}