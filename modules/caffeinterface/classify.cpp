/* Caffe Interface for deep learning
 *  Author: federico.corradi@inilabs.com
 */
#include "classify.hpp"
#include "settings.h"

using namespace caffe;
using std::string;

void MyCaffe::file_set(caerFrameEventPacketConst frameIn, bool thr, bool printOut, bool showactivations,
	bool norminput) {

	// only classify first frame, drop other for latency reasons
	caerFrameEventConst f = caerFrameEventPacketGetEventConst(frameIn, 0);

	// Initialize OpenCV Mat based on caerFrameEvent data directly (no image copy).
	cv::Size frameSize(caerFrameEventGetLengthX(f), caerFrameEventGetLengthY(f));
	cv::Mat orig(frameSize, CV_16UC(caerFrameEventGetChannelNumber(f)),
		(uint16_t *) (caerFrameEventGetPixelArrayUnsafeConst(f)));
	cv::Mat img;
	orig.convertTo(img, CV_8UC1, 1);	// convert image to gray level

	// Convert img to float for Caffe
	cv::Mat img2;
	img.convertTo(img2, CV_32FC1);
	if (norminput) {
		img2 = img2 * 0.00390625; // force normalization 0,255 to 0,1 range
	}
	// check input image
	CHECK(!img2.empty()) << "Unable to decode image " << file_i;
	std::vector<Prediction> predictions = MyCaffe::Classify(img2, 5, showactivations);

	// save predictions for low-passing decision
	lowpassed.push_back(predictions[0]);
	// decide classification result based on low-passed decision
	std::vector<int> lastDec;
	for (Prediction i : lowpassed){
		 int counter = 0;
		 for(string l : labels_){
			 if(i.first == l){
				 //caerLog(CAER_LOG_NOTICE, __func__, "item %d " , counter );
				 lastDec.push_back(counter);
			 }else{
				 counter++;
			 }
		 }
	}
	//decision index is majority vote among lasts n results, o(n^2)
    int max=0, mostvalue=lastDec[0];
    for(int i=0;i<lastDec.size();i++)
    {
        int co = (int)std::count(lastDec.begin(), lastDec.end(), lastDec[i]);
        if(co > max)
        {       max = co;
                mostvalue = lastDec[i];
        }
    }
	//caerLog(CAER_LOG_NOTICE, __func__, "mostvalue %d " , mostvalue );

	if(printOut){
		caerLog(CAER_LOG_NOTICE, __func__, "Classification Result is %s" , labels_[mostvalue].c_str());
	}

	// Write text on a window
	cv::Mat ImageText(240, 240, CV_8UC3, cv::Scalar(0, 0, 0));
	cv::putText(ImageText, labels_[mostvalue], cvPoint(30, 30), cv::FONT_HERSHEY_COMPLEX_SMALL, 0.8,
		cvScalar(200, 200, 250), 1, CV_AA);
	cv::imshow("Results", ImageText);
	cv::waitKey(3);

}

void MyCaffe::init_network(int lowPass) {

	lowpassed.set_capacity(lowPass);  // init circular buffer for average decision

	string model_file = NET_MODEL
	;
	string trained_file = NET_WEIGHTS
	;
	string mean_file = NET_MEAN
	;
	string label_file = NET_VAL
	;
	MyCaffe::Classifier(model_file, trained_file, mean_file, label_file);

	cv::namedWindow("Results", 0);
	cv::namedWindow("Activations", 1);
	return;

}

void MyCaffe::Classifier(const string& model_file, const string& trained_file, const string& mean_file,
	const string& label_file) {
#ifdef CPU_ONLY
	Caffe::set_mode(Caffe::CPU);
#else
	Caffe::set_mode(Caffe::GPU);
	//int current_device;
	//CUDA_CHECK(cudaGetDevice(&current_device));

#endif

	/* Load the network. */
	net_.reset(new Net<float>(model_file, TEST));
	net_->CopyTrainedLayersFrom(trained_file);

	CHECK_EQ(net_->num_inputs(), 1) << "Network should have exactly one input.";
	CHECK_EQ(net_->num_outputs(), 1) << "Network should have exactly one output.";

	Blob<float>* input_layer = net_->input_blobs()[0];
	num_channels_ = input_layer->channels();
	CHECK(num_channels_ == 3 || num_channels_ == 1) << "Input layer should have 1 or 3 channels.";
	input_geometry_ = cv::Size(input_layer->width(), input_layer->height());

	/* Load the binaryproto mean file. */
	//SetMean(mean_file);
	/* Load labels. */
	std::ifstream labels(label_file.c_str());
	CHECK(labels) << "Unable to open labels file " << label_file;
	string line;
	while (std::getline(labels, line))
		labels_.push_back(string(line));

	Blob<float>* output_layer = net_->output_blobs()[0];
	CHECK_EQ(labels_.size(), output_layer->channels())
		<< "Number of labels is different from the output layer dimension.";
}

static bool PairCompare(const std::pair<float, int>& lhs, const std::pair<float, int>& rhs) {
	return lhs.first > rhs.first;
}

/* Return the indices of the top N values of vector v. */
static std::vector<int> Argmax(const std::vector<float>& v, int N) {
	std::vector<std::pair<float, int> > pairs;
	for (size_t i = 0; i < v.size(); ++i)
		pairs.push_back(std::make_pair(v[i], i));
	std::partial_sort(pairs.begin(), pairs.begin() + N, pairs.end(), PairCompare);

	std::vector<int> result;
	for (int i = 0; i < N; ++i)
		result.push_back(pairs[i].second);
	return result;
}

/* Return the top N predictions. */
std::vector<Prediction> MyCaffe::Classify(const cv::Mat& img, int N, bool showactivations) {
	std::vector<float> output = Predict(img, showactivations);

	N = std::min<int>(labels_.size(), N);
	std::vector<int> maxN = Argmax(output, N);
	std::vector<Prediction> predictions;
	for (int i = 0; i < N; ++i) {
		int idx = maxN[i];
		predictions.push_back(std::make_pair(labels_[idx], output[idx]));
	}

	return predictions;
}

/* Load the mean file in binaryproto format. */
void MyCaffe::SetMean(const string& mean_file) {
	BlobProto blob_proto;
	ReadProtoFromBinaryFileOrDie(mean_file.c_str(), &blob_proto);

	/* Convert from BlobProto to Blob<float> */
	Blob<float> mean_blob;
	mean_blob.FromProto(blob_proto);
	CHECK_EQ(mean_blob.channels(), num_channels_) << "Number of channels of mean file doesn't match input layer.";

	/* The format of the mean file is planar 32-bit float BGR or grayscale. */
	std::vector<cv::Mat> channels;
#ifdef CPU_ONLY
	float* data = mean_blob.mutable_cpu_data();
#else
	float* data = mean_blob.mutable_gpu_data();
#endif
	for (int i = 0; i < num_channels_; ++i) {
		/* Extract an individual channel. */
		cv::Mat channel(mean_blob.height(), mean_blob.width(), CV_32FC1, data);
		channels.push_back(channel);
		data += mean_blob.height() * mean_blob.width();
	}

	/* Merge the separate channels into a single image. */
	cv::Mat mean;
	cv::merge(channels, mean);

	/* Compute the global mean pixel value and create a mean image
	 * filled with this value. */
	cv::Scalar channel_mean = cv::mean(mean);
	mean_ = cv::Mat(input_geometry_, mean.type(), channel_mean);

}

std::vector<float> MyCaffe::Predict(const cv::Mat& img, bool showactivations) {

	Blob<float>* input_layer = net_->input_blobs()[0];
	input_layer->Reshape(1, num_channels_, input_geometry_.height, input_geometry_.width);
	/* Forward dimension change to all layers. */
	net_->Reshape();

	std::vector<cv::Mat> input_channels;
	WrapInputLayer(&input_channels);

	Preprocess(img, &input_channels);
	net_->ForwardPrefilled(); //Prefilled();

	//IF WE ENABLE VISUALIZATION IN REAL TIME (showactivations)
	//NB: this might slow down computation
	if (showactivations) {
		const vector<shared_ptr<Layer<float> > >& layers = net_->layers();

		//image vector containing all layer activations
		vector < vector<cv::Mat> > layersVector;
		std::vector<int> ntot, ctot, htot, wtot, n_image_per_layer;

		// net blobs
		const vector<shared_ptr<Blob<float>>>&this_layer_blobs =
		net_->blobs();

		// we want all activations of all layers this_layer_blobs.size()
		for (int i = 0; i < this_layer_blobs.size(); i++) {

			int n, c, h, w;
			float data;

			if (strcmp(layers[i]->type(), "Convolution") != 0 && strcmp(layers[i]->type(), "ReLU") != 0
				&& strcmp(layers[i]->type(), "Pooling") != 0 && strcmp(layers[i]->type(), "InnerProduct") != 0) {
				continue;
			}

			n = this_layer_blobs[i]->num();
			c = this_layer_blobs[i]->channels();
			h = this_layer_blobs[i]->height();
			w = this_layer_blobs[i]->width();

			// new image Vector For all Activations of this Layer
			std::vector<cv::Mat> imageVector;

			//go over all channels/filters/activations
			ntot.push_back(n);
			ctot.push_back(c);
			htot.push_back(h);
			wtot.push_back(w);
			n_image_per_layer.push_back(n * c);
			for (int num = 0; num < n; num++) {
				//go over all channels
				for (int chan_num = 0; chan_num < c; chan_num++) {
					//go over h,w produce image
					cv::Mat newImage = cv::Mat::zeros(h, w, CV_32F);
					for (int hh = 0; hh < h; hh++) {
						//go over w
						for (int ww = 0; ww < w; ww++) {
							data = this_layer_blobs[i]->data_at(num, chan_num, hh, ww);
							newImage.at<float>(hh, ww) = data;
						}
					}
					//std::cout << layers[i]->type() << std::endl;
					//cv::normalize(newImage, newImage, 0.0, 65535, cv::NORM_MINMAX, -1);
					if (strcmp(layers[i]->type(), "Convolution") == 0) {
						cv::normalize(newImage, newImage, 0.0, 255, cv::NORM_MINMAX, -1);
					}
					if (strcmp(layers[i]->type(), "ReLU") == 0) {
						cv::normalize(newImage, newImage, 0.0, 255, cv::NORM_MINMAX, -1);
					}
					if (strcmp(layers[i]->type(), "Pooling") == 0) {
						cv::normalize(newImage, newImage, 0.0, 255, cv::NORM_MINMAX, -1);
					}
					if (strcmp(layers[i]->type(), "InnerProduct") == 0) {
						;
					}
					else {
						cv::normalize(newImage, newImage, 0.0, 255, cv::NORM_MINMAX, -1);
					}
					//cv::normalize(newImage, newImage, 0.0, 65535, cv::NORM_MINMAX, -1);
					imageVector.push_back(newImage);
				}
			}
			layersVector.push_back(imageVector);
		}

		//do the graphics only plot convolutional layers
		//divide the y in equal parts , one row per layer
		int counter_y = -1, counter_x = -1;

		// mat final Frame of activations
		int sizeX = 640;
		int sizeY = 480;
		cv::Mat1f frame_activity(sizeX, sizeY);
		int size_y_single_image = floor(sizeY / layersVector.size()); // num layers
		for (int layer_num = 0; layer_num < layersVector.size(); layer_num++) { //layersVector.size()
			counter_y += 1; // count y position of image (layers)
			counter_x = -1; // reset counter_x

			// loop over all in/out filters for this layer
			for (int img_num = 0; img_num < layersVector[layer_num].size(); img_num++) {

				counter_x += 1; // count number of images on x (filters)

				int size_x_single_image = floor(sizeX / layersVector[layer_num].size());

				if (size_x_single_image <= 0) {
					caerLog(CAER_LOG_ERROR, __func__,
						"Please check your: CAFFEVISUALIZERSIZE constant. Display size too small. Not displaying activations.");
				}
				cv::Size sizeI(size_x_single_image, size_y_single_image);
				cv::Mat1f rescaled; //rescaled image

				cv::resize(layersVector[layer_num][img_num], rescaled, sizeI); //resize image
				cv::Mat data_tp = cv::Mat(rescaled.cols, rescaled.rows, CV_8UC1);
				cv::transpose(rescaled, data_tp);

				int xloc, yloc;
				xloc = (size_x_single_image) * counter_x;
				yloc = (size_y_single_image) * counter_y;

				data_tp.copyTo(
					frame_activity.rowRange(xloc, xloc + rescaled.cols).colRange(yloc, yloc + rescaled.rows));
			}
		}

		cv::Mat data_frame = cv::Mat(frame_activity.cols, frame_activity.rows, CV_32F);
		cv::transpose(frame_activity, data_frame);

		cv::imshow("Activations", data_frame);

	}	    //if show activations

	/* Copy the output layer to a std::vector */
	Blob<float>* output_layer = net_->output_blobs()[0];

#ifdef CPU_ONLY
	const float* begin = output_layer->cpu_data();
#else
	const float* begin = output_layer->gpu_data();
#endif
	const float* end = begin + output_layer->channels();

	return std::vector<float>(begin, end);
}

/* Wrap the input layer of the network in separate cv::Mat objects
 * (one per channel). This way we save one memcpy operation and we
 * don't need to rely on cudaMemcpy2D. The last preprocessing
 * operation will write the separate channels directly to the input
 * layer. */
void MyCaffe::WrapInputLayer(std::vector<cv::Mat>* input_channels) {
	Blob<float>* input_layer = net_->input_blobs()[0];

	int width = input_layer->width();
	int height = input_layer->height();
	float* input_data = input_layer->mutable_cpu_data();
	for (int i = 0; i < input_layer->channels(); ++i) {
		cv::Mat channel(height, width, CV_32FC1, input_data);
		input_channels->push_back(channel);
		input_data += width * height;
	}
}

void MyCaffe::Preprocess(const cv::Mat& img, std::vector<cv::Mat>* input_channels) {
	/* Convert the input image to the input image format of the network. */

	// std::cout << " Preprocess --- img.channnels() " << img.channels() << ", num_channels_" << num_channels_ << std::endl;
	cv::Mat sample;
	if (img.channels() == 3 && num_channels_ == 1)
		cv::cvtColor(img, sample, cv::COLOR_BGR2GRAY);
	else if (img.channels() == 4 && num_channels_ == 1)
		cv::cvtColor(img, sample, cv::COLOR_BGRA2GRAY);
	else if (img.channels() == 4 && num_channels_ == 3)
		cv::cvtColor(img, sample, cv::COLOR_BGRA2BGR);
	else if (img.channels() == 1 && num_channels_ == 3)
		cv::cvtColor(img, sample, cv::COLOR_GRAY2BGR);
	else
		sample = img;

	cv::Mat sample_resized;
	if (sample.size() != input_geometry_)
		cv::resize(sample, sample_resized, input_geometry_);
	else
		sample_resized = sample;

	cv::Mat sample_float;
	if (num_channels_ == 3)
		sample_resized.convertTo(sample_float, CV_32FC3);
	else
		sample_resized.convertTo(sample_float, CV_32FC1);

	cv::Mat sample_normalized;
	mean_ = cv::Mat::zeros(1, 1, CV_64F); //TODO remove, compute mean_ from mean_file and adapt size for subtraction.
	//std::cout << " Preprocess: mean_size " << mean_.size() << std::endl;
	//std::cout << " num_channels_:  " << num_channels_ << std::endl;
	//std::cout << "input_channels->at(0).data" << reinterpret_cast<float*>(input_channels->at(0).data) << 
	//		"or " << net_->input_blobs()[0]->gpu_data() << std::endl;

	//std::cout << " sample_resized " << sample_resized.size() << std::endl;

	cv::subtract(sample_float, mean_, sample_normalized);

	/* This operation will write the separate BGR planes directly to the
	 * input layer of the network because it is wrapped by the cv::Mat
	 * objects in input_channels. */
	cv::split(sample_normalized, *input_channels);

	CHECK(reinterpret_cast<float*>(input_channels->at(0).data)
#ifdef CPU_ONLY
		== net_->input_blobs()[0]->cpu_data())
#else
		== net_->input_blobs()[0]->gpu_data())
#endif
		<< "Input channels are not wrapping the input layer of the network.";
}

