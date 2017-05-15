/*
 * zs_monitor_cnn_layer.cpp
 *
 *  Created on: Nov 10, 2016
 *      Author: asa
 */

#include "zs_monitor_cnn_layer.h"
#include "npp_std_func_pkg.cpp"
#include  "npp_log_utilities.cpp"
#include <vector>
#include "stdio.h"
#include "string.h"
#include "cstdint"
#include "inttypes.h"
#include <unistd.h>

zs_monitor_cnn_layer::zs_monitor_cnn_layer(FILE *l_net_file) {

    read_layer_from_file(l_net_file);

}

void zs_monitor_cnn_layer::read_layer_from_file(FILE* l_net_file) {
    read_layer_config(l_net_file);
    weights = read_weights(l_net_file);
    biases = read_biases(l_net_file);

}

std::vector<std::vector<std::vector<std::vector<int64_t>>> >zs_monitor_cnn_layer::read_weights(FILE* l_net_file) {
    //weights are indexed internally as [kernel_idx][channel][column][row]
    //weights are listed in the file as (fastest changing) column - row  - channel -kernel_idx (slower changing)

    std::vector<std::vector<std::vector<std::vector<int64_t>>>> l_weights;

    l_weights.resize(num_output_channels);
    int64_t counter_debug = 0;
    for(unsigned int kernel_idx = 0; kernel_idx < num_output_channels; kernel_idx++) {
        l_weights[kernel_idx].resize(num_input_channels);

        for(unsigned int input_channel_idx = 0; input_channel_idx < num_input_channels; input_channel_idx++) {
            l_weights[kernel_idx][input_channel_idx].resize(kernel_side);

            for(unsigned int row_idx = 0; row_idx < kernel_side; row_idx++) {

                l_weights[kernel_idx][input_channel_idx][row_idx].resize(kernel_side);

                for(unsigned int column_idx = 0; column_idx < kernel_side; column_idx++) {
                    int weight = npp_std::read_int_from_file(l_net_file);
                    int64_t weight64bit = weight;
                    l_weights[kernel_idx][input_channel_idx][row_idx][column_idx]= weight64bit;

                    //  log_utilities::debug("Weight 16b: %d - Weight 64b:%lld",weight,weight64bit );
                }
            }
        }
    }

    return (l_weights);
}

std::vector<int64_t> zs_monitor_cnn_layer::read_biases(FILE* l_net_file) {

    std::vector<int64_t> l_biases;
    for (unsigned int kernel_idx = 0; kernel_idx < num_output_channels; kernel_idx++) {
        l_biases.push_back(npp_std::read_int_from_file(l_net_file));
        //int bias = npp_std::read_int_from_file(l_net_file);
        //l_biases.push_back(0);
    }
    return (l_biases);
}

void zs_monitor_cnn_layer::read_layer_config(FILE*l_net_file) {

//Read layer config
    layer_type = npp_std::read_int_from_file(l_net_file);
    compression_enabled = npp_std::read_int_from_file(l_net_file);
    kernel_side = npp_std::read_int_from_file(l_net_file);
    num_input_channels = npp_std::read_int_from_file(l_net_file);
    num_input_columns = npp_std::read_int_from_file(l_net_file);
    num_input_rows = npp_std::read_int_from_file(l_net_file);
    num_output_channels = npp_std::read_int_from_file(l_net_file);
    pooling_enabled = npp_std::read_int_from_file(l_net_file);
    relu_enabled = npp_std::read_int_from_file(l_net_file);
    padding = npp_std::read_int_from_file(l_net_file);
    stride = npp_std::read_int_from_file(l_net_file);

    if (stride > 1) {
        log_utilities::error("Current monitor implementation doesnt support stride different from 1");
    }
    num_output_columns = (num_input_columns - kernel_side + 1 + padding * 2) / (pooling_enabled + 1);

    num_output_rows = (num_input_rows - kernel_side + 1 + padding * 2) / (pooling_enabled + 1);

}
