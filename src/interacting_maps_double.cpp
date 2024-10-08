#include <interacting_maps_double.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <sstream>
#include <cassert>
#include <numeric>
#include "Instrumentor.h"
#include <cmath>

#define PI 3.14159265
#define EXECUTE_TEST 0
#define SMALL_MATRIX_METHOD 1

// Define DEBUG_LOG macro that logs with function name in debug mode
#ifdef DEBUG
#define DEBUG_LOG(message) \
        std::cout << "DEBUG (" << __func__ << "): " << message << std::endl << \
        "###########################################" << std::endl;
#else
#define DEBUG_LOG(message) // No-op in release mode
#endif

#define PROFILING 1
#if PROFILING
#define PROFILE_SCOPE(name) InstrumentationTimer timer##__LINE__(name)
#define PROFILE_FUNCTION() PROFILE_SCOPE(__FUNCTION__)
// #define PROFILE_FUNCTION() PROFILE_SCOPE(__FUNCSIG__) (Includes call attributes, whole signature of function)
#else
#define PROFILE_SCOPE(name)
#define PROFILE_FUNCTION()
#endif

namespace fs = std::filesystem;

namespace Eigen{
    typedef Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> MatrixXdRowMajor;
    typedef Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> MatrixXdRowMajor;
    typedef Eigen::Matrix<int, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> MatrixXiRowMajor;
    typedef Eigen::Tensor<double,2,Eigen::RowMajor> Tensor2d;
    typedef Eigen::Tensor<double,3,Eigen::RowMajor> Tensor3d;
}

std::ostream& operator << (std::ostream &os, const Event &e) {
    return (os << "Time: " << e.time << " Coords: " << e.coordinates[0] << " " << e.coordinates[1] << " Polarity: " << e.polarity);
}

std::string Event::toString() const {
    std::stringstream ss;
    ss << (*this);
    return ss.str();
}

std::vector<std::string> split_string(std::stringstream sstream, char delimiter){
    std::string segment;
    std::vector<std::string> seglist;

    while(std::getline(sstream, segment, delimiter))
    {
        seglist.push_back(segment);
    }

    return seglist;
}

// GRADIENT CALCUATIONS

Eigen::VectorXd gradient(const Eigen::VectorXd& x) {
    int n = x.size();
    Eigen::VectorXd grad(n);

    // Central differences in the interior
    for (int i = 1; i < n - 1; ++i) {
        grad(i) = (x(i + 1) - x(i - 1)) / 2.0;
    }

    // Forward difference at the start
    grad(0) = x(1) - x(0);

    // Backward difference at the end
    grad(n - 1) = x(n - 1) - x(n - 2);

    return grad;
}

Eigen::MatrixXdRowMajor gradient_x(const Eigen::MatrixXdRowMajor& mat) {
    int rows = mat.rows();
    int cols = mat.cols();
    Eigen::MatrixXdRowMajor grad_x(rows-1, cols-1);
//    // Compute central differences for interior points
//    grad_x.block(1, 0, rows - 2, cols) = (mat.block(2, 0, rows - 2, cols) - mat.block(0, 0, rows - 2, cols)) / 2.0;
//    // Compute forward difference for the first row
//    grad_x.row(0) = mat.row(1) - mat.row(0);
//    // Compute backward difference for the last row
//    grad_x.row(rows - 1) = mat.row(rows - 1) - mat.row(rows - 2);

//    grad_x.block(0,0,1,cols-1) = mat.block(1,0,1,cols-1) - mat.block(0,0,1,cols-1);
    grad_x = mat.block(1,0,rows-1,cols-1) - mat.block(0,0,rows-1,cols-1);
    return grad_x;
}

Eigen::MatrixXdRowMajor gradient_y(const Eigen::MatrixXdRowMajor& mat) {
    int rows = mat.rows();
    int cols = mat.cols();
    Eigen::MatrixXdRowMajor grad_y(rows-1, cols-1);
//    // Compute central differences for interior points
//    grad_y.block(0, 1, rows, cols - 2) = (mat.block(0, 2, rows, cols - 2) - mat.block(0, 0, rows, cols - 2)) / 2.0;
//    // Compute forward difference for the first column
//    grad_y.col(0) = mat.col(1) - mat.col(0);
//    // Compute backward difference for the last column
//    grad_y.col(cols - 1) = mat.col(cols - 1) - mat.col(cols - 2);
    grad_y = mat.block(0,1,rows-1,cols-1) - mat.block(0,0,rows-1,cols-1);
    return grad_y;
}

// TENSOR CASTING

Eigen::VectorXd Tensor2Vector(const Eigen::Tensor<double,1>& input) {
    Eigen::array<Eigen::Index, 1> dims = input.dimensions();
    const double* data_ptr = input.data();
    Eigen::Map<const Eigen::VectorXd> result(data_ptr, dims[0]);
    return result;
}

Eigen::VectorXd Tensor2VectorRM(const Eigen::Tensor<double,1,Eigen::RowMajor>& input) {
    // RowMajor Version
    Eigen::array<Eigen::Index, 1> dims = input.dimensions();
    const double* data_ptr = input.data();
    Eigen::Map<const Eigen::VectorXd> result(data_ptr, dims[0]);
    return result;
}

Eigen::Tensor<double,1> Vector2Tensor(const Eigen::VectorXd& input) {
    const int cols = input.cols();
    const double* data_ptr = input.data();
    Eigen::TensorMap<const Eigen::Tensor<double,1>> result(data_ptr, cols);
    return result;
}

Eigen::MatrixXdRowMajor Tensor2Matrix(const Eigen::Tensor<double,2,Eigen::RowMajor>& input){
    Eigen::array<Eigen::Index, 2> dims = input.dimensions();
//
//    // As OpenCV uses a RowMajor layout, all Eigen::Matrix are also RowMajor. Tensors are ColMajor by default and it is
//    // recommended to only uses ColMajor Tensors. So at conversion the layout needs to be considered.
//    Eigen::Tensor<double,2,RowMajor> row_major(dims[0], dims[1]);
//    // Swap the layout and preserve the order of the dimensions; https://eigen.tuXdamily.org/dox/unsupported/eigen_tensors.html
//    Eigen::array<int,2> shuffle({1, 0});
//    row_major = input.swap_layout().shuffle(shuffle);
    const double* data_ptr = &input(0); // Points to beginning of array;
    Eigen::Map<const Eigen::MatrixXdRowMajor> result(data_ptr, dims[0], dims[1]);
    return result;
}

Eigen::Tensor<double,2> Matrix2Tensor(const Eigen::MatrixXdRowMajor& input) {
    // Returns ColMajorData

    // Get Pointer to data
    double const *data_ptr = &input(0);
    // Map data to Tensor
    Eigen::TensorMap<const Eigen::Tensor<double,2,Eigen::RowMajor>> result(data_ptr, input.rows(), input.cols());
    // Swap the layout and preserve the order of the dimensions
    array<int, 2> shuffle({1, 0});
    Tensor<double, 2> col_major_result =  result.swap_layout().shuffle(shuffle);
    return col_major_result;
}

// OPENCV PART

cv::Mat eigenToCvMat(const Eigen::MatrixXdRowMajor& eigen_matrix) {
    // Map Eigen matrix data to cv::Mat without copying
    return cv::Mat(eigen_matrix.rows(), eigen_matrix.cols(), CV_32F, (void*)eigen_matrix.data());
}

cv::Mat eigenToCvMatCopy(const Eigen::MatrixXdRowMajor& eigen_matrix) {
    // Create a cv::Mat and copy Eigen matrix data into it
    cv::Mat mat(eigen_matrix.rows(), eigen_matrix.cols(), CV_32F);
    for (int i = 0; i < eigen_matrix.rows(); ++i) {
        for (int j = 0; j < eigen_matrix.cols(); ++j) {
            mat.at<double>(i, j) = eigen_matrix(i, j);
        }
    }
    return mat;
}

Eigen::MatrixXdRowMajor cvMatToEigen(const cv::Mat& mat) {
    // Ensure the cv::Mat has the correct type
    CV_Assert(mat.type() == CV_32F);
    Eigen::array<Eigen::Index, 2> dims;
    dims[0] = mat.rows;
    dims[1] = mat.cols;
    const double* data_ptr = mat.ptr<double>();
    Eigen::Map<const Eigen::MatrixXdRowMajor> result(data_ptr, dims[0], dims[1]);
    return result;
}

Eigen::MatrixXdRowMajor cvMatToEigenCopy(const cv::Mat& mat) {
    // Ensure the cv::Mat has the correct type
    CV_Assert(mat.type() == CV_32F);
    Eigen::MatrixXdRowMajor eigen_matrix(mat.rows, mat.cols);
    for (int i = 0; i < mat.rows; ++i) {
        for (int j = 0; j < mat.cols; ++j) {
            eigen_matrix(i, j) = mat.at<double>(i, j);
        }
    }
    return eigen_matrix;
}

cv::Mat convertTodouble(cv::Mat& mat) {
    // Ensure the source matrix is of type CV_8U
    CV_Assert(mat.type() == CV_8U);

    // Convert the source matrix to CV_32F
    mat.convertTo(mat, CV_32F, 1.0 / 255.0); // Scaling from [0, 255] to [0, 1]

    return mat;
}

//TODO ensure Tensors are actually const
bool isApprox(Tensor<double,3,Eigen::RowMajor>& t1, Tensor<double,3,Eigen::RowMajor>& t2, const double precision = 1e-8){
    Map<VectorXd> mt1(t1.data(), t1.size());
    Map<VectorXd> mt2(t2.data(), t2.size());
    return mt1.isApprox(mt2, precision);
}

bool isApprox(Tensor<double,2,Eigen::RowMajor>& t1, Tensor<double,2,Eigen::RowMajor>& t2, const double precision = 1e-8){
    Map<VectorXd> mt1(t1.data(), t1.size());
    Map<VectorXd> mt2(t2.data(), t2.size());
    return mt1.isApprox(mt2, precision);
}

Eigen::MatrixXdRowMajor undistort_frame(const Eigen::MatrixXdRowMajor& frame, const cv::Mat& camera_matrix, const cv::Mat& distortion_parameters) {
    cv::Mat image = eigenToCvMat(frame);
    return cvMatToEigen(undistort_image(image, camera_matrix, distortion_parameters));
}

cv::Mat undistort_image(const cv::Mat& image, const cv::Mat& camera_matrix, const cv::Mat& distortion_parameters) {
    // cv::Mat new_camera_matrix;
    // cv::Rect roi;
    // new_camera_matrix = cv::getOptimalNewCameraMatrix(camera_matrix, distortion_parameters, cv::Size(width, height), 1, cv::Size(width, height), &roi);
    cv::Mat undistorted_image;
    cv::undistort(image, undistorted_image, camera_matrix, distortion_parameters, camera_matrix);
    return undistorted_image;
}

std::vector<cv::Mat> undistort_images(const std::vector<cv::Mat>& images, const Eigen::MatrixXdRowMajor& camera_matrix, const Eigen::MatrixXdRowMajor& distortion_coefficients) {
    std::vector<cv::Mat> undistorted_images;

    // Convert Eigen matrices to cv::Mat
    cv::Mat camera_matrix_cv = eigenToCvMat(camera_matrix);
    cv::Mat distortion_coefficients_cv = eigenToCvMat(distortion_coefficients);

    for (const auto& image : images) {
        undistorted_images.push_back(undistort_image(image, camera_matrix_cv, distortion_coefficients_cv));
    }
    return undistorted_images;
}

// Function to convert Eigen::MatrixXdRowMajor to grayscale image
cv::Mat frame2grayscale(const Eigen::MatrixXdRowMajor& frame) {
    // Convert Eigen::MatrixXdRowMajor to cv::Mat
    cv::Mat frame_cv = eigenToCvMat(frame);

    // Find min and max polarity
    double min_polarity, max_polarity;
    cv::minMaxLoc(frame_cv, &min_polarity, &max_polarity);
    DEBUG_LOG("Min Polarity: " << min_polarity)
    DEBUG_LOG("Max Polarity: " << max_polarity)

    // Normalize the frame
    cv::Mat normalized_frame;
    frame_cv.convertTo(normalized_frame, CV_32FC3, 1.0 / (max_polarity - min_polarity), -min_polarity / (max_polarity - min_polarity));

    // Scale to 0-255 and convert to CV_8U
    cv::Mat grayscale_frame;
    normalized_frame.convertTo(grayscale_frame, CV_8UC1, 255.0);

    return grayscale_frame;
}

cv::Mat V2image(const Eigen::MatrixXdRowMajor& V) {
    // Determine the shape of the image
    int rows = V.rows();
    int cols = V.cols();

    // Create an empty image with 3 channels (BGR)
    cv::Mat image = cv::Mat::zeros(rows, cols, CV_8UC3);

    // Process on_events (V > 0)
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            if (V(i, j) > 0) {
                image.at<cv::Vec3b>(i, j)[0] = 255; // Set blue channel
            }
            if (V(i, j) < 0) {
                image.at<cv::Vec3b>(i, j)[2] = 255; // Set red channel
            }
        }
    }

    return image;
}


cv::Mat vector_field2image(const Eigen::Tensor<double,3,Eigen::RowMajor>& vector_field) {
    // oben rechts: blau
    // oben links: pink
    // unten rechts: grün
    // unten links: gelb/orange


    const int rows = vector_field.dimension(0);
    const int cols = vector_field.dimension(1);

    // Calculate angles and saturations
    MatrixXdRowMajor angles(rows, cols);
    MatrixXdRowMajor saturations(rows, cols);

    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            double x = vector_field(i, j, 0);
            double y = vector_field(i, j, 1);
            angles(i, j) = std::atan2(y, x);
            saturations(i, j) = std::sqrt(x * x + y * y);
        }
    }

    // Normalize angles to [0, 179]
    cv::Mat hue(rows, cols, CV_8UC1);
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            hue.at<uint8_t>(i, j) = static_cast<uint8_t>((angles(i, j) + M_PI) / (2 * M_PI) * 179);
        }
    }

    // Normalize saturations to [0, 255]
    double max_saturation = saturations.maxCoeff();
    cv::Mat saturation(rows, cols, CV_8UC1);
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            saturation.at<uint8_t>(i, j) = static_cast<uint8_t>(std::min(saturations(i, j) / max_saturation * 255.0, 255.0));
        }
    }

    // Value channel (full brightness)
    cv::Mat value(rows, cols, CV_8UC1, cv::Scalar(255));

    // Merge HSV channels
    std::vector<cv::Mat> hsv_channels = { hue, saturation, value };
    cv::Mat hsv_image;
    cv::merge(hsv_channels, hsv_image);

    // Convert HSV image to BGR format
    cv::Mat bgr_image;
    cv::cvtColor(hsv_image, bgr_image, cv::COLOR_HSV2BGR);

    return bgr_image;
}

cv::Mat create_VIFG(const MatrixXdRowMajor& V, const MatrixXdRowMajor& I, const Tensor<double,3,Eigen::RowMajor>& F, const Tensor<double,3,Eigen::RowMajor>& G, const std::string& name = "VIFG", bool save = false) {
    cv::Mat V_img = V2image(V);
    cv::Mat I_img = frame2grayscale(I);
    cv::Mat F_img = vector_field2image(F);
    cv::Mat G_img = vector_field2image(G);

    long rows = V.rows();
    long cols = V.cols();
    cv::Mat image(rows * 2 + 20, cols * 2 + 20, CV_8UC3, cv::Scalar(0, 0, 0));

    V_img.copyTo(image(cv::Rect(5, 5, cols, rows)));
    cvtColor(I_img, I_img, cv::COLOR_GRAY2BGR);
    I_img.copyTo(image(cv::Rect(cols + 10, 5, cols+1, rows+1)));
    G_img.copyTo(image(cv::Rect(5, rows + 10, cols, rows)));
    F_img.copyTo(image(cv::Rect(cols + 10, rows + 10, cols, rows)));

    if (save && !name.empty()) {
        imwrite(name, image);
    }

    return image;
}


// INTERACTING MAPS

fs::path create_folder_and_update_gitignore(const std::string& folder_name) {
    // Get the absolute path of the current working directory
    fs::path current_directory = fs::current_path();

    // Create the output folder if it does not exist
    fs::path output_folder_path = current_directory / "output";
    // Create the directory if it does not exist
    if (!fs::exists(output_folder_path)) {
        fs::create_directory(output_folder_path);
    }

    // Same for the actual folder
    fs::path folder_path = output_folder_path / folder_name;
    if (!fs::exists(folder_path)) {
        fs::create_directory(folder_path);
    }

    // Path to the .gitignore file
    fs::path gitignore_path = current_directory/ ".gitignore";

    // Check if the folder is already in .gitignore
    bool folder_in_gitignore = false;
    if (fs::exists(gitignore_path)) {
        std::ifstream gitignore_file(gitignore_path);
        std::string line;
        while (std::getline(gitignore_file, line)) {
            if (line == folder_name || line == "/" + folder_name) {
                folder_in_gitignore = true;
                break;
            }
        }
    }

    // Add the folder to .gitignore if it's not already there
    if (!folder_in_gitignore) {
        std::ofstream gitignore_file(gitignore_path, std::ios_base::app);
        gitignore_file << "\n" << folder_name << "\n";
    }

    // Return the absolute path of the new folder
    return folder_path;
}

void read_calib(const std::string& file_path, std::vector<double>& calibration_data){
    fs::path current_directory = fs::current_path();
    std::string path = current_directory / file_path;
    if (fs::exists(path)) {
        std::ifstream calibration_file(path);
        std::string::size_type size;
        for (std::string line; std::getline(calibration_file, line, ' ');) {
            calibration_data.push_back(std::stof(line, &size));
        }
    }
}

Calibration_Data get_calibration_data(const std::vector<double>& raw_data, int frame_height, int frame_width){
    Calibration_Data data;
    data.focal_point = std::vector<double>(raw_data.begin(), raw_data.begin()+2);
    data.camera_matrix = Eigen::MatrixXd({{data.focal_point[0], 0, raw_data[2]},
                                              {0, data.focal_point[1], raw_data[3]},
                                              {0, 0, 1}});
    data.distortion_coefficients = std::vector<double>(raw_data.begin()+4, raw_data.end());
    data.view_angles = std::vector<double>({2*std::atan(frame_height/(2*data.focal_point[0])),
                                            2*std::atan(frame_width/(2*data.focal_point[1]))});
    return data;
}

void read_events(const std::string& file_path, std::vector<Event>& events, double start_time, double end_time, int max_events = INT32_MAX){
    fs::path current_directory = fs::current_path();
    std::string path = current_directory / file_path;
    if (fs::exists(path)) {
        std::ifstream event_file(path);
        int counter;
        double time;
        int width, height, polarity;
        while (event_file >> time >> width >> height >> polarity){
            if (time < start_time) continue;
            if (time > end_time) break;
            if (counter > max_events) break;
            Event event;
            event.time = time;
            std::vector<int> coords = {height, width};
            event.coordinates = coords;
            event.polarity = polarity;
            events.push_back(event);
            counter++;
        }
        DEBUG_LOG("Final time stamp: " << time)
        DEBUG_LOG("Number of events: " << events.size())
    }
}

std::vector<std::vector<Event>> bin_events(std::vector<Event>& events, double bin_size = 0.05){
    std::vector<std::vector<Event>> bins;
    if (events.empty()) {
        return bins;  // Return empty if the input vector is empty
    }

    Event minVal = events.front();  // The lowest number in the sorted vector
    double currentBinStart = minVal.time;

    std::vector<Event> currentBin;

    for (Event event : events) {
        if (event.time >= currentBinStart && event.time < currentBinStart + bin_size) {
            currentBin.push_back(event);
        } else {
            // Push the current bin into bins and start a new bin
            bins.push_back(currentBin);
            currentBin.clear();
            currentBinStart += bin_size;
            // Keep adjusting the currentBinStart if the number falls outside the current bin
            while (event.time >= currentBinStart + bin_size) {
                currentBinStart += bin_size;
                bins.emplace_back(); // Add an empty bin for skipped bins
            }
            currentBin.push_back(event);
        }
    }
    // Push the last bin
    bins.push_back(currentBin);
    return bins;
}

void create_frames(const std::vector<std::vector<Event>>& bucketed_events, std::vector<Tensor<double,2,Eigen::RowMajor>>& frames, const int camera_height, const int camera_width){
    int i = 0;
    Tensor<double,2,Eigen::RowMajor> frame(camera_height, camera_width);
    Tensor<double,2,Eigen::RowMajor> cum_frame(camera_height, camera_width);
    for (std::vector<Event> event_vector : bucketed_events){

        frame.setZero();
        cum_frame.setZero();
        for (Event event : event_vector){
//            std::cout << event << std::endl;
            frame(event.coordinates.at(0), event.coordinates.at(1)) = (double)event.polarity*2-1;
//            cum_frame(event.coordinates.at(0), event.coordinates.at(1)) += (double)event.polarity;
        }
        frames[i] = frame;
        i++;

        DEBUG_LOG("Eventvector size: " << event_vector.size());
        DEBUG_LOG("Last Event: " << event_vector.back());
    }

}

void create_sparse_matrix(const int N, const Tensor<double,2,Eigen::RowMajor>& V, SpMat& result){
    // Step 1: Create the repeated identity matrix part
    std::vector<Triplet<double>> tripletList;
    tripletList.reserve(N*3*2);
    for (int i = 0; i<N*3; i++){
        tripletList.push_back(Triplet<double>(i,i%3,1.0));
    }

    // Step 2: Create the diagonal and off-diagonal values from V
    int j = 3;
    array<long,1> one_dim{{V.size()}};
    Eigen::Tensor<double,1,Eigen::RowMajor> k = V.reshape(one_dim);
    for (int i = 0; i<N*3; i++){
        tripletList.push_back(Triplet<double>(i,j,k(i)));
        if (i%3 == 2){
            j++;
        }
    }
    result.setFromTriplets(tripletList.begin(), tripletList.end());
}

void norm_tensor_along_dim3(const Tensor<double,3,Eigen::RowMajor>& T, Tensor<double,2,Eigen::RowMajor>& norm){
    array<int,1> dims({2});
    norm = T.square().sum(dims).sqrt();
}

// Function to compute C_star
autodiff::Vector3real C_star(autodiff::real x, autodiff::real y, int N_x, int N_y, double height, double width, double rs) {
    autodiff::Vector3real result;
    result << height * (-1 + (2 * x) / (N_x - 1)),
              width * (1 - (2 * y) / (N_y - 1)),
              rs;
    return result;
}

// Function to compute C
autodiff::Vector3real C(autodiff::real x, autodiff::real y, int N_x, int N_y, double height, double width, double rs) {
    autodiff::Vector3real c_star = C_star(x, y, N_x, N_y, height, width, rs);
    autodiff::real norm = sqrt(c_star.squaredNorm());
    return c_star / norm;
}

// Jacobian for x value tested by hand
void find_C(int N_x, int N_y, double view_angle_x, double view_angle_y, double rs, Tensor<double,3,Eigen::RowMajor>& CCM, Tensor<double,3,Eigen::RowMajor>& C_x, Tensor<double,3,Eigen::RowMajor>& C_y) {
    double height = tan(view_angle_x / 2);
    double width = tan(view_angle_y / 2);

    DEBUG_LOG("view_angle_x: " << view_angle_x);
    DEBUG_LOG("view_angle_y: " << view_angle_y);
    DEBUG_LOG("Height: " << height);
    DEBUG_LOG("Width: " << width);
    // Create grid of points
    Eigen::MatrixXd XX(N_x, N_y);
    Eigen::MatrixXd YY(N_x, N_y);
    for (int i = 0; i < N_x; ++i) {
        for (int j = 0; j < N_y; ++j) {
            XX(i, j) = i;
            YY(i, j) = j;
        }
    }
//    std::cout << "X Grid: " << XX << std::endl;
//    std::cout << "Y Grid: " << YY << std::endl;


    // Compute the camera calibration map (CCM) and the Jacobians
    // std::vector<std::vector<autodiff::Vector3real>> CCM;
    // Tensor<double,3,Eigen::RowMajor> CCM_T;
    // Tensor<double,3,Eigen::RowMajor> C_x;
    // Tensor<double,3,Eigen::RowMajor> C_y;
    // std::vector<std::vector<Eigen::VectorXd>> C_y;
    for (int i = 0; i < N_x; ++i) {
        for (int j = 0; j < N_y; ++j) {
            autodiff::real x = XX(i, j);
            autodiff::real y = YY(i, j);

            // Compute the function value
            autodiff::Vector3real c_val = C(x, y, N_x, N_y, height, width, rs);
            CCM(i,j,0) = static_cast<double>(c_val(0));
            CCM(i,j,1) = static_cast<double>(c_val(1));
            CCM(i,j,2) = static_cast<double>(c_val(2));
            // Compute the Jacobians
            // Vector3real dCdx;
            // Vector3real dCdy;
            autodiff::VectorXreal F;

            // NEEDS TO STAY D O U B L E
            Eigen::VectorXd dCdx = autodiff::jacobian(C, wrt(x), at(x,y,N_x, N_y, height, width, rs), F);
            Eigen::VectorXd dCdy = autodiff::jacobian(C, wrt(y), at(x,y,N_x, N_y, height, width, rs), F);

            C_x(i,j,0) = dCdx(0);
            C_x(i,j,1) = dCdx(1);
            C_x(i,j,2) = dCdx(2);
            C_y(i,j,0) = dCdy(0);
            C_y(i,j,1) = dCdy(1);
            C_y(i,j,2) = dCdy(2);

//            std::cout << "CCM (i: " << i << "), (j: " << j << "):" << CCM << std::endl;
//            std::cout << "C_x (i: " << i << "), (j: " << j << "):" << C_x << std::endl;
//            std::cout << "C_y (i: " << i << "), (j: " << j << "):" << C_y << std::endl;

            // C_x(i,j,0) = 1.0f;
            // C_x(i,j,1) = 1.0f;
            // C_x(i,j,2) = 1.0f;
            // C_y(i,j,0) = 1.0f;
            // C_y(i,j,1) = 1.0f;
            // C_y(i,j,2) = 1.0f;
        }
    }
}

void crossProduct3x3(const Eigen::Tensor<double,3,Eigen::RowMajor>& A, const Eigen::Tensor<double,3,Eigen::RowMajor>& B, Eigen::Tensor<double,3,Eigen::RowMajor>& C) {
    assert(A.dimensions() == B.dimensions() && "Tensors A and B must have the same shape");

    // Use Eigen's tensor operations to compute the cross product
    C.chip(0, 2) = A.chip(1, 2) * B.chip(2, 2) - A.chip(2, 2) * B.chip(1, 2);
    C.chip(1, 2) = A.chip(2, 2) * B.chip(0, 2) - A.chip(0, 2) * B.chip(2, 2);
    C.chip(2, 2) = A.chip(0, 2) * B.chip(1, 2) - A.chip(1, 2) * B.chip(0, 2);
}

void crossProduct1x3(const Eigen::Tensor<double,1>& A, const Eigen::Tensor<double,3,Eigen::RowMajor>& B, Eigen::Tensor<double,3,Eigen::RowMajor>& C){
    // Use Eigen's tensor operations to compute the cross product
    for (int i = 0; i < B.dimension(0); ++i) {
        for (int j = 0; j < B.dimension(1); ++j) {
            C(i, j, 0) = A(1) * B(i, j, 2) - A(2) * B(i, j, 1);
            C(i, j, 1) = A(2) * B(i, j, 0) - A(0) * B(i, j, 2);
            C(i, j, 2) = A(0) * B(i, j, 1) - A(1) * B(i, j, 0);
        }
    }
}

// Function to compute the cross product of two 3-tensors
void crossProduct3x3_loop(const Eigen::Tensor<double,3,Eigen::RowMajor>& A, const Eigen::Tensor<double,3,Eigen::RowMajor>& B, Eigen::Tensor<double,3,Eigen::RowMajor>& C) {
    assert(A.dimensions() == B.dimensions() && "Tensors A and B must have the same shape");
    for (int i = 0; i < A.dimension(0); ++i) {
        for (int j = 0; j < A.dimension(1); ++j) {
            C(i, j, 0) = A(i, j, 1) * B(i, j, 2) - A(i, j, 2) * B(i, j, 1);
            C(i, j, 1) = A(i, j, 2) * B(i, j, 0) - A(i, j, 0) * B(i, j, 2);
            C(i, j, 2) = A(i, j, 0) * B(i, j, 1) - A(i, j, 1) * B(i, j, 0);
        }
    }
}

void vector_distance(const Tensor<double,3,Eigen::RowMajor> &vec1, const Tensor<double,3,Eigen::RowMajor> &vec2, Tensor<double,2,Eigen::RowMajor> &distance){
    const auto& dimensions = vec1.dimensions();
    Tensor<double,3,Eigen::RowMajor> cross_product(dimensions);
    Tensor<double,2,Eigen::RowMajor> norm(dimensions[0], dimensions[1]);
    Tensor<double,2,Eigen::RowMajor> norm2(dimensions[0], dimensions[1]);
    crossProduct3x3(vec1, vec2, cross_product);
    norm_tensor_along_dim3(cross_product, norm);
    norm_tensor_along_dim3(vec2, norm2);
//    std::cout << vec1 << " " << vec2 << std::endl;
//    std::cout << "cross product " << cross_product << std::endl;
//    std::cout << "norm " << norm << std::endl;
//    std::cout << "norm2 " << norm2 << std::endl;
    distance = norm/norm2;
}

double sign_func(double x){
    // Apply via a.unaryExpr(std::ptr_fun(sign_func))
    if (x > 0)
        return +1.0;
    else if (x == 0)
        return 0.0;
    else
        return -1.0;
}

// Function using nested loops to compute the dot product
void computeDotProductWithLoops(const Eigen::Tensor<double,3,Eigen::RowMajor>& A, const Eigen::Tensor<double,3,Eigen::RowMajor>& B, Eigen::Tensor<double,2,Eigen::RowMajor>& D) {
    const int m = A.dimension(0);
    const int n = A.dimension(1);
    const int d = A.dimension(2);

    // #pragma omp parallel for collapse(2)
    for (int i = 0; i < m; ++i) {
        for (int j = 0; j < n; ++j) {
            double dotProduct = 0.0f; // Initialize the dot product for position (i, j)
            for (int k = 0; k < d; ++k) {
                dotProduct += A(i, j, k) * B(i, j, k);
            }
            D(i, j) = dotProduct; // Store the result in tensor D
        }
    }
}

void m32(const Tensor<double,3,Eigen::RowMajor> &In, const Tensor<double,3,Eigen::RowMajor> &C_x, const Tensor<double,3,Eigen::RowMajor> &C_y, Tensor<double,3,Eigen::RowMajor> &Out){
    const auto& dimensions = In.dimensions();
    Tensor<double,3,Eigen::RowMajor> C1(dimensions);
    Tensor<double,3,Eigen::RowMajor> C2(dimensions);
    Tensor<double,2,Eigen::RowMajor> dot(dimensions[0], dimensions[1]);
    Tensor<double,2,Eigen::RowMajor> sign(dimensions[0], dimensions[1]);
    Tensor<double,2,Eigen::RowMajor> distance1(dimensions[0], dimensions[1]);
    Tensor<double,2,Eigen::RowMajor> distance2(dimensions[0], dimensions[1]);

//    std::cout << "In " << In << std::endl;
//    std::cout << "C_x " << C_x << std::endl;
//    std::cout << "C_y " << C_y << std::endl;

    crossProduct3x3(C_x,C_y,C1);
    crossProduct3x3(C_y,C1,C2);
    computeDotProductWithLoops(In,C2,dot);
    sign = dot.unaryExpr(std::ptr_fun(sign_func));
    vector_distance(In, C_y, distance1);
    vector_distance(C_x, C_y, distance2);
    Out.chip(0,2) = sign * distance1/distance2;

//    std::cout << "C1 " << C1 << std::endl;
//    std::cout << "C2 " << C2 << std::endl;
//    std::cout << "dot " << dot << std::endl;
//    std::cout << "sign" << sign << std::endl;
//    std::cout << "distance1 " << distance1 << std::endl;
//    std::cout << "distance2 " << distance2 << std::endl;
//    std::cout << "Out " << Out << std::endl;

    crossProduct3x3(C_y,C_x,C1);
    crossProduct3x3(C_x,C1,C2);
    computeDotProductWithLoops(In,C2,dot);
    sign = dot.unaryExpr(std::ptr_fun(sign_func));
    vector_distance(In, C_x, distance1);
    vector_distance(C_y, C_x, distance2);
    Out.chip(1,2) = sign * distance1/distance2;

//    std::cout << "C1 " << C1 << std::endl;
//    std::cout << "C2 " << C2 << std::endl;
//    std::cout << "dot " << dot << std::endl;
//    std::cout << "sign" << sign << std::endl;
//    std::cout << "distance1 " << distance1 << std::endl;
//    std::cout << "distance2 " << distance2 << std::endl;
//    std::cout << "Out " << Out << std::endl;
}

void m23(const Tensor<double,3,Eigen::RowMajor>& In, const Tensor<double,3,Eigen::RowMajor>& Cx, const Tensor<double,3,Eigen::RowMajor>& Cy, Tensor<double,3,Eigen::RowMajor>& Out) {
    const auto& dimensions = Cx.dimensions();
    for (int i = 0; i < dimensions[0]; i++){
        for (int j = 0; j < dimensions[1]; j++){
            Out(i,j,0) = In(i,j,0) * Cx(i,j,0) + In(i,j,1) * Cy(i,j,0);
            Out(i,j,1) = In(i,j,0) * Cx(i,j,1) + In(i,j,1) * Cy(i,j,1);
            Out(i,j,2) = In(i,j,0) * Cx(i,j,2) + In(i,j,1) * Cy(i,j,2);
        }
    }
}
// // Function using .chip() to compute the dot product
// void computeDotProductWithChip(const Eigen::Tensor<double,3,Eigen::RowMajor>& A, const Eigen::Tensor<double,3,Eigen::RowMajor>& B, Eigen::Tensor<double,2,Eigen::RowMajor>& D) {
//     const int m = A.dimension(0);
//     const int n = A.dimension(1);
//     Tensor<double, 1> a_slice;
//     Tensor<double, 1> b_slice;
//     Tensor<double, 0> res;
//     for (int i = 0; i < m; ++i) {
//         for (int j = 0; j < n; ++j) {
//             // Extract the slices and compute the dot product
//             a_slice = A.chip(i, 0).chip(j, 0);
//             b_slice = B.chip(i, 0).chip(j, 0);
//             res = (a_slice * b_slice).sum().sqrt();
//             D(i, j) = res(); // Compute the dot product
//         }
//     }
// }

// Function to time the performance of a given dot product computation function
template<typename Func>
void timeDotProductComputation(Func func, const Eigen::Tensor<double,3,Eigen::RowMajor>& A, const Eigen::Tensor<double,3,Eigen::RowMajor>& B, Eigen::Tensor<double,2,Eigen::RowMajor>& D, int iterations) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        func(A, B, D);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Time elapsed: " << elapsed.count() << " seconds" << std::endl;
}


void update_F_from_G(Tensor<double,3,Eigen::RowMajor>& F, Tensor<double,2,Eigen::RowMajor>& V, Tensor<double,3,Eigen::RowMajor>& G, const double lr, const double weight_FG){
    InstrumentationTimer timer("update_F_from_G");
    const auto& dimensions = F.dimensions();
    Tensor<double,3,Eigen::RowMajor> update_F(dimensions);
    for (int i = 0; i<dimensions[0]; i++){
        for (int j = 0; j<dimensions[1]; j++){
            update_F(i,j,0) = F(i,j,0) - ((G(i,j,0)/(G(i,j,0) * G(i,j,0) + G(i,j,1) * G(i,j,1))) * (V(i,j) + (F(i,j,0) * G(i,j,0) + F(i,j,1) * G(i,j,1))));
            update_F(i,j,1) = F(i,j,1) - ((G(i,j,1)/(G(i,j,0) * G(i,j,0) + G(i,j,1) * G(i,j,1))) * (V(i,j) + (F(i,j,0) * G(i,j,0) + F(i,j,1) * G(i,j,1))));
        }
    }
    F = (1-weight_FG)*F + lr * weight_FG * update_F;
}

void update_G_from_F(Tensor<double,3,Eigen::RowMajor>& G, Tensor<double,2,Eigen::RowMajor>& V, Tensor<double,3,Eigen::RowMajor>& F, const double lr, const double weight_GF){
    InstrumentationTimer timer("update_G_from_F");
    const auto& dimensions = G.dimensions();
    Tensor<double,3,Eigen::RowMajor> update_G(dimensions);
    for (int i = 0; i<dimensions[0]; i++){
        for (int j = 0; j<dimensions[1]; j++){
            update_G(i,j,0) = (i,j,0) - ((F(i,j,0)/(F(i,j,0) * F(i,j,0) + F(i,j,1) * F(i,j,1))) * (V(i,j) + (G(i,j,0) * F(i,j,0) + G(i,j,1) * F(i,j,1))));
            update_G(i,j,1) = G(i,j,1) - ((F(i,j,1)/(F(i,j,0) * F(i,j,0) + F(i,j,1) * F(i,j,1))) * (V(i,j) + (G(i,j,0) * F(i,j,0) + G(i,j,1) * F(i,j,1))));
        }
    }
    G = (1-weight_GF)*G + lr * weight_GF * update_G;
}

void update_G_from_I(Tensor<double,3,Eigen::RowMajor> &G, Tensor<double,3,Eigen::RowMajor> &I_gradient, const double weight_GI){
    InstrumentationTimer timer("update_G_from_I");
    G = (1 - weight_GI) * G + weight_GI*I_gradient;
}

void update_I_from_V(Tensor<double,2,Eigen::RowMajor> &I, Tensor<double,2,Eigen::RowMajor> &cum_V, const double weight_IV=0.5, const double time_step=0.05){
    InstrumentationTimer timer("update_I_from_V");
    const auto& dimensions_V = cum_V.dimensions();
    const auto& dimensions = I.dimensions();
    Eigen::array<Eigen::Index, 2> offsets = {0, 0};
    Eigen::array<Eigen::Index, 2> extents = {dimensions_V.at(0), dimensions_V.at(1)};
    DEBUG_LOG("I: " << I);
    DEBUG_LOG("Cum_V: " << cum_V);
    I.slice(offsets,extents) = I.slice(offsets,extents) + weight_IV*cum_V;
    DEBUG_LOG("I: " << I);
//    for (int i = 0; i<dimensions[0]; i++){
//        for (int j = 0; j<dimensions[1]; j++){
//            if (I(i,j)>0){
//                if (I(i,j)>time_step){
//                    I(i,j) -= time_step;
//                }
//                else{
//                    I(i,j) = 0;
//                }
//            }
//            if (I(i,j)<0){
//                if (I(i,j)<time_step){
//                    I(i,j) += time_step;
//                }
//                else{
//                    I(i,j) = 0;
//                }
//            }
//        }
//    }
}

void update_I_from_G(Tensor<double,2,Eigen::RowMajor> &I, Tensor<double,3,Eigen::RowMajor> &I_gradient, Tensor<double,3,Eigen::RowMajor> &G, const double weight_IG=0.5){
    InstrumentationTimer timer("update_I_from_G");
    const auto& dimensions = I.dimensions();
    Tensor<double,3,Eigen::RowMajor> temp_map = G - I_gradient;
    Tensor<double,2,Eigen::RowMajor> x_update(dimensions);
    Tensor<double,2,Eigen::RowMajor> y_update(dimensions);
//    std::cout << G << std::endl;
//    std::cout << I_gradient << std::endl;
//    std::cout << temp_map << std::endl;
    x_update.setZero();
    y_update.setZero();
    for (int i = 0; i<dimensions[0]-1; i++){
        for (int j = 0; j<dimensions[1]-1; j++){
            if (i==0){
//                std::cout << temp_map(i,j,0) << std::endl;
                x_update(i,j) = temp_map(i,j,0);
            }
            else{
                x_update(i,j) = temp_map(i,j,0) - temp_map(i-1,j,0);
            }
            if (j==0){
                y_update(i,j) = temp_map(i,j,1);
            }
            else{
                y_update(i,j) = temp_map(i,j,1) - temp_map(i,j-1,1);
            }
        }
    }
//    std::cout << x_update << std::endl;
//    std::cout << y_update << std::endl;
    I = (1 - weight_IG)*I + weight_IG*(I - x_update - y_update);
    // # Gradient Implementation, the paper mentions an effect in x-direction which gets computed as a difference between matrix components in x-direction.
    // # This is very similar to how gradients are implemented which is why they are used here. The temp map consists of an x and a y component since, the
    // # gradient of I consists of two components. One for the x and one for the y direction.
    // x_update = jnp.gradient(temp_map[:,:,0])
    // y_update = jnp.gradient(temp_map[:,:,1])
    // return (1 - weight_IG)*I + weight_IG*(I - x_update[0] - y_update[1])

    // Like Paper Implementation
    // x_update = np.zeros((temp_map.shape[:-1]))
    // y_update = np.zeros((temp_map.shape[:-1]))
    // x_update[0,:] = temp_map[0,:,0] # - 0 // Out of bound entries are set to 0
    // x_update[1:,:] = temp_map[1:,:,0] - temp_map[:-1,:,0]
    // y_update[:,0] = temp_map[:,0,1] # - 0 // Out of bound entries are set to 0
    // y_update[:,1:] = temp_map[:,1:,1] - temp_map[:,:-1,1]
    // return (1 - weight_IG)*I + weight_IG*(I - x_update - y_update)
}
void update_F_from_R(Tensor<double,3,Eigen::RowMajor>& F, const Tensor<double,3,Eigen::RowMajor>& CCM, const Tensor<double,3,Eigen::RowMajor>& Cx, const Tensor<double,3,Eigen::RowMajor>& Cy, const Tensor<double,1>& R, const double weight_FR){
    PROFILE_FUNCTION();
    Tensor<double,3,Eigen::RowMajor> cross(CCM.dimensions());
//    const auto& dimensions = F.dimensions();
    Tensor<double,3,Eigen::RowMajor> update(F.dimensions());
    {
        PROFILE_SCOPE("FR CROSSPRODUCT");
        crossProduct1x3(R, CCM, cross);
    }
//    std::cout << cross << std::endl;
    {
        PROFILE_SCOPE("FR M32");
        m32(cross, Cx, Cy, update);
    }
//    std::cout << update << std::endl;

    F = (1 - weight_FR)*F + weight_FR*update;
}

void update_R_from_F(Tensor<double,1>& R, const Tensor<double,3,Eigen::RowMajor>& F, const Tensor<double,3,Eigen::RowMajor>& C, const Tensor<double,3,Eigen::RowMajor>& Cx, const Tensor<double,3,Eigen::RowMajor>& Cy, SpMat& sparse_m, const double weight_RF, const int N) {
    //InstrumentationTimer timer1("update_R_from_F");
    PROFILE_FUNCTION();
    const auto &dimensions = F.dimensions();
    Tensor<double, 3, Eigen::RowMajor> transformed_F(dimensions[0], dimensions[1], 3);
    Tensor<double, 3, Eigen::RowMajor> points_tensor_3(dimensions[0], dimensions[1], 3);
    Tensor<double, 3, Eigen::RowMajor> directions_tensor_3(dimensions[0], dimensions[1], 3);
    Tensor<double, 2, Eigen::RowMajor> points_tensor_2;
    Tensor<double, 2, Eigen::RowMajor> directions_tensor_2;
    Tensor<double, 1, Eigen::RowMajor> points_tensor_1;
    Tensor<double, 1, Eigen::RowMajor> directions_tensor_1;
    Eigen::Vector3d solution_short(3);
    Eigen::VectorXd solution_long(N*3);
    Eigen::array<int , 2> reshaper_2({N, 3});
    Eigen::array<int , 1> reshaper_1({N * 3});
    Eigen::MatrixXd points_matrix;
    Eigen::MatrixXd directions_matrix;
    Eigen::VectorXd points_vector;
    Eigen::VectorXd directions_vector;

    if (SMALL_MATRIX_METHOD){
        Matrix3d A = Matrix3d::Zero();
        Vector3d B = Vector3d::Zero();
        Matrix3d I = Matrix3d::Identity();
        Matrix3d outerProduct;
        Eigen::Vector<double,3> p;
        Eigen::Vector<double,3> d;
        Eigen::Vector<double,3> d1;
        {
            PROFILE_SCOPE("RF Pre");
            m23(F, Cx, Cy, transformed_F);
            crossProduct3x3(C, transformed_F, points_tensor_3);
            points_tensor_2 = points_tensor_3.reshape(reshaper_2); // reshaped_points need to be Eigen::Vector for solver
            points_matrix = Tensor2Matrix(points_tensor_2);
            directions_tensor_2 = C.reshape(reshaper_2);
            directions_matrix = Tensor2Matrix(directions_tensor_2);
//            std::cout << transformed_F << std::endl;
//            std::cout << points_tensor_3 << std::endl;
//            std::cout << points_tensor_2 << std::endl;
//            std::cout << points_matrix << std::endl;
//            std::cout << C << std::endl;
//            std::cout << directions_tensor_2 << std::endl;
//            std::cout << directions_matrix << std::endl;
            for (size_t i = 0; i < directions_matrix.rows(); ++i){
                p = points_matrix.block<1,3>(i,0);
//                d = directions_matrix.block<1,3>(i,0).normalized(); // Normalize direction vector
                d = directions_matrix.block<1,3>(i,0);
                outerProduct = d * d.transpose();
//                std::cout << p << std::endl;
//                std::cout << d << std::endl;
//                std::cout << outerProduct << std::endl;
                A += I - outerProduct;
                B += (I - outerProduct) * p;
            }
            d1 = directions_matrix.block<1,3>(0,0);
            DEBUG_LOG("First direction: " << std::endl << d1);
            DEBUG_LOG("Last direction: " << std::endl << d);
            DEBUG_LOG("N outer products: " << std::endl << directions_matrix.rows());
            DEBUG_LOG("outerProduct: " << std::endl << outerProduct);
            DEBUG_LOG("A: " << std::endl << A);
            DEBUG_LOG("B: " << std::endl << B);
        }
        solution_short = A.partialPivLu().solve(B);
//        std::cout << "R update: " << solution_short << std::endl;
        R(0) = (1-weight_RF)*R(0) + weight_RF*solution_short(0);
        R(1) = (1-weight_RF)*R(1) + weight_RF*solution_short(1);
        R(2) = (1-weight_RF)*R(2) + weight_RF*solution_short(2);
        if (std::abs(R(0)) < 1e-14 or std::isnan(R(0))){
            R(0) = 0.0;
        }
        if (std::abs(R(1)) < 1e-14 or std::isnan(R(1))){
            R(1) = 0.0;
        }
        if (std::abs(R(2)) < 1e-14 or std::isnan(R(2))){
            R(2) = 0.0;
        }
    }
    else{
            {
                PROFILE_SCOPE("RF PRE");
                m23(F, Cx, Cy, transformed_F);
                crossProduct3x3(C, transformed_F, points_tensor_3);
                points_tensor_1 = points_tensor_3.reshape(reshaper_1); // reshaped_points need to be Eigen::Vector for solver
                points_vector = Tensor2VectorRM(points_tensor_1);
                directions_tensor_1 = C.reshape(reshaper_1);
//                std::cout << transformed_F << std::endl;
//                std::cout << points_tensor_3 << std::endl;
//                std::cout << points_tensor_1 << std::endl;
//                std::cout << points_vector << std::endl;
//                std::cout << C << std::endl;
//                std::cout << directions_tensor_1 << std::endl;

            }

            {
                PROFILE_SCOPE("RF Set Entries");
                int counter = 0;
                for (int k=3; k<sparse_m.outerSize(); ++k)
                    // Skip the first three cols as they stay the same
                    for (SparseMatrix<double>::InnerIterator it(sparse_m,k); it; ++it)
                    {
                        // Iterate Columnwise through the matrix. Meaning the first entries are all 1 until the 4th column is reached
                        // This also happens exactly at have the contained values -> 3N ones, 3N reshaped_Cs
                        // j = it.value();
                        it.valueRef() = directions_tensor_1[counter];
                        counter++;
                    }
            }

        Eigen::LeastSquaresConjugateGradient<Eigen::SparseMatrix<double>, Eigen::LeastSquareDiagonalPreconditioner<double>> solver; // Tends to fail really often
        // The Paper Nearest approaches to multiple lines in n-dimensional space from Han and Bancroft mention not to use least square methods on the multiple lines
        // problem. Instead they recommend a SVD approach.

        solver.setTolerance(1e-4);
//        solver.setMaxIterations(2000); // Set with no basis
        {
            // Perform the computation and measure the time
            PROFILE_SCOPE("SOLVE");

            solver.analyzePattern(sparse_m); // TODO: Needs only to be executed once since sparsity pattern does not change
            solver.factorize(sparse_m); // Needs to be done every iteration
            if (solver.info() != Eigen::Success) {
                std::cerr << "Decomposition failed during update_R_from_C" << std::endl;
            }

            // x = solver.solve(b);
            solution_long = solver.solveWithGuess(points_vector.cast<double>(), solution_long);
            if (solver.info() != Eigen::Success) {
                std::cerr << "Solving failed during update_R_from_C " << std::endl;
                DEBUG_LOG("Errenous vector RF: " << points_vector({0,1,2,3,4,5,6,7,8}));
            }
            else {
//            std::cout << "Solver took " << solver.iterations() << " steps." << std::endl;
                R(0) = (1-weight_RF)*R(0) + weight_RF*solution_long(0);
                R(1) = (1-weight_RF)*R(1) + weight_RF*solution_long(1);
                R(2) = (1-weight_RF)*R(2) + weight_RF*solution_long(2);
            }
        }
    }
}

// TODO: add return number
void interacting_maps_step(Tensor<double,2,Eigen::RowMajor>& V, Tensor<double,2,Eigen::RowMajor>& cum_V, Tensor<double,2,Eigen::RowMajor>& I, Tensor<double,3,Eigen::RowMajor>& F, Tensor<double,3,Eigen::RowMajor>& G, Tensor<double,1>& R, const Tensor<double,3,Eigen::RowMajor>& CCM, const Tensor<double,3,Eigen::RowMajor>& dCdx, const Tensor<double,3,Eigen::RowMajor>& dCdy, SpMat& sparse_m, std::unordered_map<std::string,double>& weights, std::vector<int>& permutation, const int N){
    PROFILE_FUNCTION();
    Eigen::array<Eigen::Index, 2> dimensions = V.dimensions();
    Eigen::Tensor<double,3,Eigen::RowMajor> delta_I(dimensions[0], dimensions[1], 2);
    array<int, 2> shuffle({1, 0});
    delta_I.chip(0,2) = Matrix2Tensor(gradient_x(Tensor2Matrix(I))).swap_layout().shuffle(shuffle); // Swap Layout of delta_I_x back 2 RowMajor as Matrix2Tensor returns ColMajor.
    delta_I.chip(1,2) = Matrix2Tensor(gradient_y(Tensor2Matrix(I))).swap_layout().shuffle(shuffle);
    for (const auto& element : permutation){
        switch( element ){
            default:
                std::cout << "Unknown number in permutation" << std::endl;
            case 0:
//                DEBUG_LOG("F: "<< std::endl << F);
                update_F_from_G(F, V, G, weights["lr"], weights["weight_FG"]);
//                DEBUG_LOG("F: "<<  F);
//                DEBUG_LOG("V: "<<  V);
//                DEBUG_LOG("G: "<<  G);
//                    std::cout << "Case " << element << std::endl;
                break;
            case 1:
//                std::cout << F << std::endl;
                update_F_from_R(F, CCM, dCdx, dCdy, R, weights["weight_FR"]);
//                std::cout << F << std::endl;
//                std::cout << CCM << std::endl;
//                std::cout << dCdx << std::endl;
//                std::cout << dCdy << std::endl;
//                std::cout << R << std::endl;
//                    std::cout << "Case " << element << std::endl;
                break;
            case 2:
//                std::cout << G << std::endl;
                update_G_from_F(G, V, F, weights["lr"], weights["weight_FG"]);
//                std::cout << G << std::endl;
//                std::cout << V << std::endl;
//                std::cout << F << std::endl;
//                    std::cout << "Case " << element << std::endl;
                break;
            case 3:
//                std::cout << G << std::endl;
                update_G_from_I(G, delta_I, weights["weight_GI"]);
//                std::cout << G << std::endl;
//                std::cout << delta_I << std::endl;
//                    std::cout << "Case " << element << std::endl;
                break;
            case 4:
//                std::cout << I << std::endl;
                update_I_from_G(I, delta_I, G, weights["weight_IG"]);
                delta_I.chip(0, 2) = Matrix2Tensor(gradient_x(Tensor2Matrix(I))).swap_layout().shuffle(shuffle);
                delta_I.chip(1, 2) = Matrix2Tensor(gradient_y(Tensor2Matrix(I))).swap_layout().shuffle(shuffle);
//                std::cout << I << std::endl;
//                std::cout << G << std::endl;
//                std::cout << delta_I << std::endl;
//                    std::cout << "Case " << element << std::endl;
                break;
            case 5:
//                std::cout << I << std::endl;
                update_I_from_V(I, V, weights["weight_IV"], weights["time_step"]);
                delta_I.chip(0, 2) = Matrix2Tensor(gradient_x(Tensor2Matrix(I))).swap_layout().shuffle(shuffle);
                delta_I.chip(1, 2) = Matrix2Tensor(gradient_y(Tensor2Matrix(I))).swap_layout().shuffle(shuffle);
//                std::cout << I << std::endl;
//                std::cout << V << std::endl;
//                std::cout << delta_I << std::endl;
//                    std::cout << "Case " << element << std::endl;
                break;
            case 6:
//                std::cout << R << std::endl;
                update_R_from_F(R, F, CCM, dCdx, dCdy, sparse_m, weights["weight_RF"], N);
//                std::cout << R << std::endl;
//                std::cout << F << std::endl;
//                std::cout << CCM << std::endl;
//                std::cout << dCdx << std::endl;
//                std::cout << dCdy << std::endl;
//                    std::cout << "Case " << element << std::endl;
                break;
         }
     }
 }

int test(){
    //##################################################################################################################
    // TEST PARAMETERS
    int n = 2;
    int m = 2;
    int nm = n*m;
    int N = 50;
    int M = 70;
    int NM = N*M;
    bool test_conversion = true;
    bool test_update = true;
    bool test_helper = true;
    bool test_file = true;
    bool test_step = true;
    bool test_cv = true;

    if (test_step){
        test_helper = true;
    }

    //##################################################################################################################
    // Camera calibration matrix (C/CCM) and dCdx/dCdy
    Tensor<double,3,Eigen::RowMajor> CCM(n,m,3);
    CCM.setZero();
    Tensor<double,3,Eigen::RowMajor> dCdx(n,m,3);
    dCdx.setZero();
    Tensor<double,3,Eigen::RowMajor> dCdy(n,m,3);
    dCdy.setZero();

    //##################################################################################################################
    // Optic flow F, temporal derivative V, spatial derivative G
    Tensor<double,3,Eigen::RowMajor> F(n,m,2);
    F.setZero();
    F.chip(1,2).setConstant(1.0);
    Tensor<double,2,Eigen::RowMajor> V(n,m);
    V.setZero();
    Tensor<double,3,Eigen::RowMajor> G(n,m,2);
    G.setZero();
    G.chip(1,2).setConstant(-1.0);

    //##################################################################################################################
    // Intesity I
    Tensor<double,2,Eigen::RowMajor> I(n+1,m+1);
    I.setZero();
    Tensor<double,3,Eigen::RowMajor> I_gradient(n,m,2);
    I_gradient.setRandom();

    //##################################################################################################################
    // Rotation Vector R
    Tensor<double,1> R(3);
    R.setRandom();

    if (test_conversion){
        std::cout << "TESTING CONVERSION" << std::endl;
        //##############################################################################################################
        // Test Tensor casts
        Tensor<double,2,Eigen::RowMajor> T2M (N,M);
        Tensor<double,1> T2V (N);
        Tensor<double,2> M2T_res (N,M);
        Tensor<double,1> V2T_res (N);
        T2M.setConstant(1.0);
        T2V.setConstant(1.0);
        Eigen::MatrixXdRowMajor M2T (N,M);
        Eigen::VectorXd V2T (N);
        Eigen::MatrixXdRowMajor T2M_res (N,M);
        Eigen::VectorXd T2V_res (N);
        M2T.setConstant(2.0);
        V2T.setConstant(2.0);
        T2M_res = Tensor2Matrix(T2M);
        T2V_res = Tensor2Vector(T2V);
        M2T_res = Matrix2Tensor(M2T);
        V2T_res = Vector2Tensor(V2T);
        std::cout << "Implemented Tensor/Matrix/Vector casts" << std::endl;

        //##################################################################################################################
        // TESTING OPENCV CONVERSION
        Eigen::MatrixXdRowMajor eigen_matrix = Eigen::MatrixXdRowMajor::Constant(3, 3, 2.0);
        cv::Mat mat = eigenToCvMat(eigen_matrix);
        std::cout << "Implemented Eigen to CV" << std::endl << mat << std::endl;

        cv::Mat mat2(3, 3, CV_32F, cv::Scalar::all(1));
        Eigen::MatrixXdRowMajor eigen_matrix2 = cvMatToEigen(mat2);
        std::cout << "Implemented CV to eigen" << std::endl << eigen_matrix2 << std::endl;

        std::cout << "CONVERSION TEST PASSED" << std::endl;
    }

    if (test_update) {
        std::cout << "TESTING UPDATE FUNCTIONS" << std::endl;
        //##################################################################################################################
        // Update F/G from G/F
        Tensor<double,3,Eigen::RowMajor> F_comparison(n,m,2);
        F.setConstant(0.0);
        F.chip(1,2).setConstant(1.0);
        V(0,0) = 1;
        V(0,1) = 2;
        V(1,0) = 3;
        V(1,1) = 4;
        G.setConstant(0.0);
        G.chip(1,2).setConstant(-1.0);
        F_comparison.setConstant(0.0);
        F_comparison(0,0,1) = 1;
        F_comparison(0,1,1) = 1.5;
        F_comparison(1,0,1) = 2.0;
        F_comparison(1,1,1) = 2.5;
        update_F_from_G(F, V, G, 1.0, 0.5);
        if(isApprox(F, F_comparison)){
            std::cout << "UPDATE FUNCTION FG CORRECT" << std::endl;
        }else{
            std::cout << "UPDATE FUNCTION FG FALSE" << std::endl;
            std::cout << "F after update" << std::endl;
            std::cout << F << std::endl;
            std::cout << "F should be" << std::endl;
            std::cout << F_comparison << std::endl;
        }
        //##################################################################################################################
        // gradient I
        Tensor<double,2,Eigen::RowMajor> I_gradient_comparison(n,m);

        I.setValues({{0,1,3}, {1,3,6}, {3,6,10}});
        I_gradient_comparison.setValues({{1,2},{2,3}});
        G.chip(1,2).setConstant(1.0);
        Eigen::array<Eigen::Index, 2> dimensions = I.dimensions();
        Eigen::Tensor<double,2,Eigen::RowMajor> delta_I_x(dimensions[0], dimensions[1]);
        Eigen::Tensor<double,2,Eigen::RowMajor> delta_I_y(dimensions[0], dimensions[1]);
        array<int, 2> shuffle({1, 0});
        I_gradient.chip(0,2) = Matrix2Tensor(gradient_x(Tensor2Matrix(I))).swap_layout().shuffle(shuffle); // Swap Layout of delta_I_x back 2 RowMajor as Matrix2Tensor returns ColMajor.
        I_gradient.chip(1,2) = Matrix2Tensor(gradient_y(Tensor2Matrix(I))).swap_layout().shuffle(shuffle);
        delta_I_x = I_gradient.chip(0,2);
        delta_I_y = I_gradient.chip(1,2);
        if (isApprox(I_gradient_comparison, delta_I_x) and isApprox(I_gradient_comparison, delta_I_y)){
            std::cout << "I GRADIENT FUNCTION CORRECT" << std::endl;
        }else{
            std::cout << "I GRADIENT FUNCTION FALSE" << std::endl;
            std::cout << "I gradient x after update" << std::endl;
            std::cout << delta_I_x << std::endl;
            std::cout << "I gradient y after update" << std::endl;
            std::cout << delta_I_y << std::endl;
            std::cout << "I gradient should be" << std::endl;
            std::cout << I_gradient_comparison << std::endl;
        }

        //##################################################################################################################
        // Update I from G
        Tensor<double,2,Eigen::RowMajor> I_comparison(n+1,m+1);
        I_gradient.chip(0,2) = I_gradient_comparison;
        I_gradient.chip(1,2) = I_gradient_comparison;
        I_comparison.setValues({{1,4,3},{3,5,6},{3,6,10}});
        update_I_from_G(I, I_gradient, G, 1.0);
        if (isApprox(I_comparison, I)){
            std::cout << "UPDATE FUNCTION IG CORRECT" << std::endl;
        }else{
            std::cout << "UPDATE FUNCTION IG FALSE" << std::endl;
            std::cout << "I update" << std::endl;
            std::cout << I << std::endl;
            std::cout << "I should be" << std::endl;
            std::cout << I_comparison << std::endl;
        }

        //##################################################################################################################
        // Update I from V
        I.setValues({{0,1,3}, {1,3,6}, {3,6,10}});
        V.setValues({{1,1},{0,0}});
        I_comparison.setValues({{0.4,0.9,2.9},{0.4,1.4,5.9},{2.9,5.9,9.9}});
        update_I_from_V(I, V, 0.5, 0.1);
        if (isApprox(I_comparison, I)){
            std::cout << "UPDATE FUNCTION IV CORRECT" << std::endl;
        }else{
            std::cout << "UPDATE FUNCTION IV FALSE" << std::endl;
            std::cout << "I update" << std::endl;
            std::cout << I << std::endl;
            std::cout << "I should be" << std::endl;
            std::cout << I_comparison << std::endl;
        }

        //##################################################################################################################
        // Update G from I
        Tensor<double,3,Eigen::RowMajor> G_comparison(n,m,2);
        G.setValues({{{1,4},{2,3}},{{3,2},{4,1}}});
        I_gradient.setValues({{{1,0},{0,1}},{{0,1},{1,0}}});
        G_comparison.setValues({{{1,2},{1,2}},{{1.5,1.5},{2.5,0.5}}});
        std::cout << G << std::endl;
        std::cout << I_gradient << std::endl;
        update_G_from_I(G, I_gradient, 0.5);
        if (isApprox(G_comparison, G)){
            std::cout << "UPDATE FUNCTION GI CORRECT" << std::endl;
        }else{
            std::cout << "UPDATE FUNCTION GI FALSE" << std::endl;
            std::cout << "G update" << std::endl;
            std::cout << G << std::endl;
            std::cout << "G should be" << std::endl;
            std::cout << G_comparison << std::endl;
        }
        std::cout << "UPDATE FUNCTIONS TEST PASSED" << std::endl;
    }

    if (test_file){
        std::cout << "TESTING FILE READOUT AND EVENT HANDLING" << std::endl;
        //##################################################################################################################
        // Create results_folder
        std::string folder_name = "results";
        fs::path folder_path = create_folder_and_update_gitignore(folder_name);
        std::cout << "Implemented Folder creation" << std::endl;

        //##################################################################################################################
        // Read calibration file
        std::string calib_path = "../res/shapes_rotation/calib.txt";
        std::vector<double> calibration_data;
        read_calib(calib_path, calibration_data);
        std::cout << "Implemented calibration data readout" << std::endl;

        //##################################################################################################################
        // Read events file
        std::string event_path = "../res/shapes_rotation/events.txt";
        std::vector<Event> event_data;
        read_events(event_path, event_data, 0.0, 1.0);
        std::cout << "Implemented events readout" << std::endl;

        //##################################################################################################################
        // Bin events
        std::vector<std::vector<Event>> binned_events;
        binned_events = bin_events(event_data, 0.05);

        //##################################################################################################################
        // Create frames
        size_t frame_count = binned_events.size();
        std::vector<Tensor<double,2,Eigen::RowMajor>> frames(frame_count);
        create_frames(binned_events, frames, 180, 240);
        std::cout << "Implemented event binning and event frame creation" << std::endl;
        std::cout << "FILE READOUT AND EVENT HANDLING TEST PASSED" << std::endl;
    }

    if (test_helper){
        std::cout << "TESTING HELPER FUNCTIONS" << std::endl;
        //##################################################################################################################
        // Test sparse matrix creation
        Tensor3d In2(n,m,2);
        Tensor3d Cx(n,m,3);
        Tensor3d Cy(n,m,3);
        Tensor3d Out3(n,m,3);
        Tensor3d Out3_comparison(n,m,3);
        In2.setValues({{{1,4},{2,3}},{{3,2},{4,1}}});
        Cx.setValues({{{1,2,3},{1,2,3}},{{1,2,3},{1,2,3}}});
        Cy.setValues({{{.1,.2,.3},{.1,.2,.3}},{{.1,.2,.3},{.1,.2,.3}}});
        Out3_comparison.setValues({{{1.4,2.8,4.2},{2.3,4.6,6.9}},{{3.2,6.4,9.6},{4.1,8.2,12.3}}});
        m23(In2, Cx, Cy, Out3);
        if (isApprox(Out3_comparison, Out3)){
            std::cout << "HELPER FUNCTION M23 CORRECT" << std::endl;
        }else{
            std::cout << "HELPER FUNCTION M23 FALSE" << std::endl;
            std::cout << "Out" << std::endl;
            std::cout << Out3 << std::endl;
            std::cout << "Out should be" << std::endl;
            std::cout << Out3_comparison << std::endl;
        }

        //##################################################################################################################
        // Test Vector distance function needed for M32
        Tensor3d dCdx_small(1,1,3);
        Tensor3d dCdy_small(1,1,3);
        Tensor2d distance_comparison(1,1);
        Tensor2d distance(1,1);
        dCdx_small.setValues({{{1,2,3}}});
        dCdy_small.setValues({{{1,0,1}}});
        distance_comparison.setValues({{(double)std::sqrt(6.0)}});
        vector_distance(dCdx_small, dCdy_small, distance);
        if (isApprox(distance_comparison, distance)){
            std::cout << "HELPER FUNCTION VECTOR_DISTANCE CORRECT" << std::endl;
        }else{
            std::cout << "HELPER FUNCTION VECTOR_DISTANCE FALSE" << std::endl;
            std::cout << "distance" << std::endl;
            std::cout << distance << std::endl;
            std::cout << "distance should be" << std::endl;
            std::cout << distance_comparison << std::endl;
        }

        //##################################################################################################################
        // M32 Test
        Tensor3d In3(1,1,3);
        Tensor3d Out2(1,1,2);
        Tensor3d Out2_comparison(1,1,2);
        // TAKE dCdx and dCdy from vector_distance test above
        In3.setValues({{{1,1,1}}});
        Out2_comparison.setValues({{{(double)(1/std::sqrt(6)), (double)(std::sqrt(6)/std::sqrt(12))}}});
        m32(In3, dCdx_small, dCdy_small, Out2);
        if (isApprox(Out2_comparison, Out2, 1e-6)){
            std::cout << "HELPER FUNCTION M32 CORRECT" << std::endl;
        }else{
            std::cout << "HELPER FUNCTION M32 FALSE" << std::endl;
            std::cout << "Out2" << std::endl;
            std::cout << Out2 << std::endl;
            std::cout << "Out2 should be" << std::endl;
            std::cout << Out2_comparison << std::endl;
        }

        //##################################################################################################################
        // Camera calibration map
        find_C(n, m, PI/2, PI/2, 1.0f, CCM, dCdx, dCdy);
//        std::cout << CCM << std::endl;
//        std::cout << dCdx << std::endl;
//        std::cout << dCdy << std::endl;
//        std::cout << "Implemented find_C function with autodiff" << std::endl;
        double epsilon = 1e-6;
        if (std::abs(CCM(1,1,0) - 1/std::sqrt(3)) < epsilon and std::abs(dCdx(1,1,0) - 4*std::sqrt(3)/9) < epsilon and std::abs(dCdx(1,1,1) - 2/(std::sqrt(3)*3)) < epsilon){
            std::cout << "HELPER FUNCTION FIND_C CORRECT" << std::endl;
        }else{
            std::cout << "HELPER FUNCTION FIND_C FALSE" << std::endl;
        }


        //##################################################################################################################
        // Tensor cross product
        // Define the dimensions of the tensors
        Eigen::array<Eigen::Index, 3> dimensions = {n, m, 3};
        // Initialize tensors A and B with random values
        Eigen::Tensor<double,3,Eigen::RowMajor> A(dimensions);
        Eigen::Tensor<double,3,Eigen::RowMajor> B(dimensions);
        Eigen::Tensor<double,3,Eigen::RowMajor> D(dimensions);
        A.setRandom();
        B.setRandom();

        int iterations = 1000;

        // Timing the chip-based implementation
        std::cout << "Timing cross product for tensors implementations " << std::endl;
        auto start_chip = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            crossProduct3x3(A, B, D);
        }
        auto end_chip = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration_chip = end_chip - start_chip;
        std::cout << "Time for chip-based implementation: " << duration_chip.count()/iterations << " seconds\n";

        // Timing the loop-based implementation
        auto start_loop = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            crossProduct3x3_loop(A, B, D);
        }
        auto end_loop = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration_loop = end_loop - start_loop;
        std::cout << "Time for loop-based implementation: " << duration_loop.count()/iterations << " seconds\n";
        std::cout << "Implemented cross product for Tensors" << std::endl;
        // Define the resulting tensor D with shape (m, n)
        Eigen::Tensor<double,2,Eigen::RowMajor> E(n, m);
        // Time the dot product computation using nested loops
        std::cout << "Timing dot product computation with loops:" << std::endl;
        timeDotProductComputation(computeDotProductWithLoops, A, B, E, 1000);
        std::cout << "Implemented dot product for tensors" << std::endl;

        // // Time the dot product computation using .chip() (CHATGPT USES CHIP JUST FOR FANCY INDEXING)
        // std::cout << "Timing dot product computation with .chip():" << std::endl;
        // timeDotProductComputation(computeDotProductWithChip, A, B, E, 1000);


        //##################################################################################################################
        // Tensor cross product 1x3
        {
            Eigen::Tensor<double,1> A(3);
            Eigen::Tensor<double,3,Eigen::RowMajor> B(2,2,3);
            Eigen::Tensor<double,3,Eigen::RowMajor> C(2,2,3);
            Eigen::Tensor<double,3,Eigen::RowMajor> C_comp(2,2,3);
            A.setValues({1,2,3});
            B.setValues({{{0,1,2},{0,1,2}},{{0,1,2},{0,1,2}}});
            C_comp.setValues({{{1,-2,1},{1,-2,1}},{{1,-2,1},{1,-2,1}}});
            crossProduct1x3(A, B, C);
            if (isApprox(C_comp, C)){
                std::cout << "HELPER FUNCTION CROSSPRODUCT1x3 CORRECT" << std::endl;
            }else{
                std::cout << "HELPER FUNCTION CROSSPRODUCT1x3 FALSE" << std::endl;
                std::cout << "C" << std::endl;
                std::cout << C << std::endl;
                std::cout << "C should be" << std::endl;
                std::cout << C_comp << std::endl;
            }
        }

        std::cout << "HELPER FUNCTIONS TEST PASSED" << std::endl;
    }

    if (test_step){
        std::cout << "TESTING UPDATE STEP FUNCTION" << std::endl;

        //##################################################################################################################
        // Camera calibration matrix (C/CCM) and dCdx/dCdy
        Tensor<double,3,Eigen::RowMajor> CCM(n,m,3);
        CCM.setZero();
        Tensor<double,3,Eigen::RowMajor> dCdx(n,m,3);
        dCdx.setZero();
        Tensor<double,3,Eigen::RowMajor> dCdy(n,m,3);
        dCdy.setZero();
        find_C(n, m, PI/2, PI/2, 1.0f, CCM, dCdx, dCdy);

        //##################################################################################################################
        // Optic flow F, temporal derivative V, spatial derivative G
        Tensor<double,3,Eigen::RowMajor> F(n,m,2);
        F.setConstant(0.1);
        F.chip(1,2).setConstant(1.0);
        Tensor<double,2,Eigen::RowMajor> V(n,m);
        V.setConstant(1.0);
        Tensor<double,3,Eigen::RowMajor> G(n,m,2);
        G.setConstant(0.0);
        G.chip(1,2).setConstant(-1.0);

        //##################################################################################################################
        // Intesity I
        Tensor<double,2,Eigen::RowMajor> I(n+1,m+1);
        I.setValues({{1,4,0},{9,16,0},{0,0,0}});
        Tensor<double,3,Eigen::RowMajor> I_gradient(n,m,2);
        I_gradient.setZero();

        //##################################################################################################################
        // Rotation Vector R
        Tensor<double,1> R(3);
        R.setValues({1,2,3});

        //##################################################################################################################
        // Sparse Matrix Creation
        Tensor<double,2,Eigen::RowMajor> F_flat(nm,3);
        F_flat.setConstant(1.0);
        SpMat sparse_m(nm*3,nm+3);
        create_sparse_matrix(nm, F_flat, sparse_m);
        std::cout << "Implemented sparse matrix creation for update_R" << std::endl;
        //##################################################################################################################
        // Testing Interacting Maps step
        std::unordered_map<std::string,double> weights;
        weights["weight_FG"] = 0.2;
        weights["weight_FR"] = 0.8;
        weights["weight_GF"] = 0.2;
        weights["weight_GI"] = 0.2;
        weights["weight_IG"] = 0.2;
        weights["weight_IV"] = 0.2;
        weights["weight_RF"] = 0.8;
        weights["lr"] = 0.9;
        weights["time_step"] = 0.05f;
        std::vector<int> permutation {0,1,2,3,4,5,6};
        for (int i = 0; i < 100; ++i){
            interacting_maps_step(V, V, I, F, G, R, CCM, dCdx, dCdy, sparse_m, weights, permutation, nm);
        }
        std::cout << "R: " << R << std::endl;
        std::cout << "Implemented interacting maps update step" << std::endl;
        std::cout << "UPDATE STEP FUNCTION TEST PASSED" << std::endl;
    }

    if (test_cv){
        std::cout << "TESTING OPENCV FUNCTIONS" << std::endl;
        //##################################################################################################################
        // Convert to grayscale image
        Eigen::MatrixXdRowMajor grayscale_test = Eigen::MatrixXdRowMajor::Random(100,100);
        cv::Mat grayscale_image = frame2grayscale(grayscale_test);
        cv::imshow("Grayscale Image", grayscale_image);
//        cv::waitKey(0);

        //##################################################################################################################
        // TESTING UNDISTORT IMAGE
        // Camera matrix (3x3)
        Eigen::MatrixXdRowMajor camera_matrix_eigen(3, 3);
        camera_matrix_eigen << 800, 0, 320,
                0, 800, 240,
                0, 0, 1;
        cv::Mat camera_matrix_cv = eigenToCvMat(camera_matrix_eigen);

        // Distortion coefficients (1x5)
        Eigen::MatrixXdRowMajor distortion_coefficients_eigen(1, 5);
        distortion_coefficients_eigen << 0.1, -0.05, 0, 0, 0; // Simple distortion
        cv::Mat distortion_coefficients_cv = eigenToCvMat(distortion_coefficients_eigen);

        // Convert Eigen matrices to cv::Mat and back to Eigen
        // Test undistort_image function
        cv::Mat undistorted_image = undistort_image(grayscale_image, camera_matrix_cv, distortion_coefficients_cv);

        // Print result
        cv::imshow("Undistorted Image", undistorted_image);
//        cv::waitKey(0);

        // V to Image
        Eigen::MatrixXdRowMajor V2image_eigen = Eigen::MatrixXdRowMajor::Random(100,100);
        cv::Mat V2image_cv = V2image(V2image_eigen);
        cv::imshow("V Image", V2image_cv);
//        cv::waitKey(0);

        // Vectorfield to Image
        Eigen::Tensor<double,3,Eigen::RowMajor> vector_field(1000,1000,2);
        vector_field.setRandom();
        cv::Mat image = vector_field2image(vector_field);
        cv::imshow("Vector Field Image", image);
//        cv::waitKey(0);

        // Matrix V I F G to OpenCV Image
        Eigen::MatrixXdRowMajor Vimage = Eigen::MatrixXdRowMajor::Random(100, 100);  // Example data
        Eigen::MatrixXdRowMajor Iimage = Eigen::MatrixXdRowMajor::Random(100+1, 100+1);  // Example data
        Eigen::Tensor<double,3,Eigen::RowMajor> Fimage(100, 100, 2);          // Example data
        Eigen::Tensor<double,3,Eigen::RowMajor> Gimage(100, 100, 2);          // Example data

        // Fill example tensor data (normally you would have real data)
        for (int i = 0; i < 100; ++i) {
            for (int j = 0; j < 100; ++j) {
                Fimage(i, j, 0) = std::sin(i * 0.1);
                Fimage(i, j, 1) = std::cos(j * 0.1);
                Gimage(i, j, 0) = std::cos(i * 0.1);
                Gimage(i, j, 1) = std::sin(j * 0.1);
            }
        }
        cv::Mat result = create_VIFG(Vimage, Iimage, Fimage, Gimage, "output.png", true);
        cv::imshow("Result", result);
//        cv::waitKey(0);
        std::cout << "OPENCV TEST PASSED" << std::endl;
    }
    return 0;
}

int main() {
    //##################################################################################################################
    // Parameters
    if (EXECUTE_TEST){
        test();
    }
    else{
        auto clock_time = std::chrono::system_clock::now();
        std::time_t time = std::chrono::system_clock::to_time_t(clock_time);
        std::string results_name = "results ";
        std::string folder_name = results_name + std::ctime(&time);
        std::string calib_path = "../res/shapes_rotation/calib.txt";
        std::string event_path = "../res/shapes_rotation/events.txt";

        double start_time_events = 10.0; // in s
        double end_time_events = 10.05; // in s
        double time_bin_size_in_s = 0.05; // in s
        int iterations = 1000;

        int height = 180; // in pixels
        int width = 240; // in pixels

        std::unordered_map<std::string,double> weights;
        weights["weight_FG"] = 0.2;
        weights["weight_FR"] = 0.8;
        weights["weight_GF"] = 0.2;
        weights["weight_GI"] = 0.2;
        weights["weight_IG"] = 0.2;
        weights["weight_IV"] = 0.5;
        weights["weight_RF"] = 0.8;
        weights["lr"] = 0.9;
        weights["time_step"] = time_bin_size_in_s;
        std::vector<int> permutation {0,1,2,4,5}; // Which update steps to take

        //##################################################################################################################
        // Optic flow F, temporal derivative V, spatial derivative G, intensity I, rotation vector R
        Tensor<double,3,Eigen::RowMajor> F(height, width, 2);
        F.setConstant(0.1);
        Tensor<double,3,Eigen::RowMajor> G(height, width,2);
        G.setConstant(0.1);
        Tensor<double,2,Eigen::RowMajor> I(height+1, width+1);
        I.setConstant(0.0);
        Tensor<double,3,Eigen::RowMajor> I_gradient(height, width,2);
        I_gradient.setZero();
        Tensor<double,1> R(3);
        R.setConstant(0.1);


        //##################################################################################################################
        // Create results_folder
        fs::path folder_path = create_folder_and_update_gitignore(folder_name);
        std::cout << "Created Folder " << folder_name << std::endl;

        //##################################################################################################################
        // Read calibration file
        std::vector<double> raw_calibration_data;
        read_calib(calib_path, raw_calibration_data);
        Calibration_Data calibration_data = get_calibration_data(raw_calibration_data, height, width);
        std::cout << "Readout calibration file at " << calib_path << std::endl;

        //##################################################################################################################
        // Read events file
        std::vector<Event> event_data;
        read_events(event_path, event_data, start_time_events, end_time_events);
        std::cout << "Readout events at " << event_path << std::endl;

        //##################################################################################################################
        // Bin events
        std::vector<std::vector<Event>> binned_events;
        binned_events = bin_events(event_data, time_bin_size_in_s);
        std::cout << "Binned events" << std::endl;

        //##################################################################################################################
        // Create frames
        size_t frame_count = binned_events.size();
        std::vector<Tensor<double,2,Eigen::RowMajor>> frames(frame_count);
        create_frames(binned_events, frames, height, width);
        std::cout << "Created frames out of events" << std::endl;

        //##################################################################################################################
        // Camera calibration matrix (C/CCM) and dCdx/dCdy
        Tensor<double,3,Eigen::RowMajor> CCM(height, width,3);
        CCM.setZero();
        Tensor<double,3,Eigen::RowMajor> dCdx(height, width,3);
        dCdx.setZero();
        Tensor<double,3,Eigen::RowMajor> dCdy(height, width,3);
        dCdy.setZero();
        find_C(height, width, calibration_data.view_angles[0], calibration_data.view_angles[1], 1.0f, CCM, dCdx, dCdy);
        std::cout << "Calculated Camera Matrix" << std::endl;

        std::string profiler_name = "Profiler.json";
        fs::path profiler_path = folder_path / profiler_name;
        Instrumentor::Get().BeginSession("Interacting Maps", profiler_path);
        std::cout << "Setup Profiler" << std::endl;
        int counter = 0;

        // Setup Sparse Matrix for update_R_from_F
        int N = height*width;
        std::vector<Eigen::Triplet<double>> tripletList;
        SpMat sparse_m(N * 3, N + 3);
        tripletList.reserve(6 * N); // Assuming on average 2 non-zero entries per row
        int j = 3;
        for (int i = 0; i < N * 3; i++) {
            tripletList.emplace_back(i, i % 3, 1.0); // Diagonal element
            j = int(i/3 + 3);
            tripletList.emplace_back(i, j, 1.0); // Points, Placeholder
        }
        sparse_m.setFromTriplets(tripletList.begin(), tripletList.end());
        for (Tensor<double,2,Eigen::RowMajor> V : frames){
//            std::cout << V << std::endl;
            for (int iter = 0; iter < iterations; ++iter){
                interacting_maps_step(V, V, I, F, G, R, CCM, dCdx, dCdy, sparse_m, weights, permutation, N);
                if (iter%100==0){
                    std::cout << iter << "/" << iterations << std::endl;
                }
            }
            std::cout << "R: " << R << std::endl;
            counter++;
            std::cout << "Frame: " << counter << std::endl;
            std::string image_name = "VIFG_" + std::to_string(counter) + ".png";
            fs::path image_path = folder_path / image_name;
            create_VIFG(Tensor2Matrix(V), Tensor2Matrix(I), F, G, image_path, true);
        }
        Instrumentor::Get().EndSession();
    }

}



