#ifndef __ZS_AXIDMA_CPP__
#define __ZS_AXIDMA_CPP__

#include "zs_axi_dma_lib.hpp"

void* write_thread_routine(void* arg) {
    usleep(100 * 1000); //TODO remove. Only for test lookpback example
    log_utilities::high("Creating write thread...");
    ZS_axidma* zsaxidma = (ZS_axidma*) arg;
    while (zsaxidma->is_write_thread_running()) {

        if ( !zsaxidma->write_data.empty()) {
            try {
                //Here the mutex act just like a semaphore
                pthread_mutex_lock( &zsaxidma->write_mxt);

                //Here we dont need to keep the mutex lock: data inside front are fine thanks to previous semaphore
                //And now we are just reading from the list, not modifying it
                //usleep(10); //TODO remove. Only for test lookpback example
                zsaxidma->axidma.write_splitted( &zsaxidma->write_data.front());

                //Here we modify the list, so we need to lock it
                //pthread_mutex_lock(&zsaxidma->write_mxt);
                zsaxidma->write_data.pop_front();
                //usleep(500); //TODO remove. Only for test lookpback example
                pthread_mutex_unlock( &zsaxidma->write_mxt);
            } catch (AXIDMA_timeout_exception& ex) {
                log_utilities::error(ex.what());
                log_utilities::error("Write thread timeout");
                // exit(-1); //TODO REMOVE ME
                zsaxidma->stop();
                //zsaxidma->init(zsaxidma->axidma.get_read_transfer_length_bytes());
            } catch (std::bad_alloc& ba) {
                log_utilities::error("bad_alloc caught: %s. List size --> %d", ba.what(), zsaxidma->write_data.size());
            }
        } else {
            //This microsleep is necessary to avoid the loop to iterate infinitely locking the CPU
            //Notice that it is MANDATORY keep it here to allow the mutex inside the if{} statement, otherwise the system will lock
            //into an infinite loop. If mutex are moved outside the if, the usleep can be remove but performance decrease will occour
            //value obtained trying multiple times
            usleep(70);
        }
    }

    log_utilities::high("Destroying write thread...");
    pthread_detach(pthread_self()); //Necessary to avoid memory leak
    pthread_exit(NULL);
}

void ZS_axidma::write(const std::vector<uint64_t> *data) {
    pthread_mutex_lock( &write_mxt);
    write_data.push_back( *data);
    pthread_mutex_unlock( &write_mxt);

}

ZS_axidma::ZS_axidma() :
        axidma(AXIDMA_DEVICE_DEFINE, SOURCE_ADDR_OFFSET_DEFINE, DESTINATION_ADDR_OFFSET_DEFINE), axigpio(AXIGPIO_BASE) {
    //read_layer_finish = false;
    pthread_mutex_init( &write_mxt, NULL);
    write_thread = 1;
    write_thread_running = false;
}

ZS_axidma::~ZS_axidma(void) {
    pthread_cancel(write_thread);
}

void ZS_axidma::init(unsigned int read_tranfer_length) {
    reset();
    write_data.clear();
    log_utilities::high("Initializing ZS_axidma using %d bytes as read transfer length", read_tranfer_length);
    if ( !axidma.init(read_tranfer_length)) {
        exit( -1);
    }
    pthread_create( &write_thread, NULL, write_thread_routine, (void*) this);
    write_thread_running = true;
}

void ZS_axidma::reset(void) {
    axidma.reset();
    axigpio.set_gpio_direction("out");
    axigpio.set_gpio_value(0x01);
}

void ZS_axidma::stop(void) {
    write_thread_running = false;
    usleep(100);
    axidma.stop();
}

int ZS_axidma::readLayer(std::vector<uint64_t> *layer_data) {
    log_utilities::high("Launching read operation...");
    layer_data->clear();

        try {
            axidma.read(layer_data);

            /*printf("First data: 0x%llx and Last data: 0x%llx of the read layer. Number of data: %d\n", layer_data->front(), layer_data->back(), layer_data->size());
             std::vector<uint64_t>::iterator it;
             for(it=layer_data->begin(); it<layer_data->end(); it++)
             {
             printf("Layer_data %d: 0x%llx\n", std::distance(layer_data->begin(), it), *it);
             }*/
        } catch (AXIDMA_timeout_exception& ex) {
            log_utilities::error(ex.what());
            log_utilities::error("Read thread timeout");
            log_utilities::debug("First data: 0x%llx and Last data: 0x%llx of the read layer", layer_data->front(),
                    layer_data->back());
            //sleep(5); // TODO remove me, just for debug
            stop();
            //exit(-1); //TODO REMOVE ME
            init(axidma.get_read_transfer_length_bytes());
            //sleep(5); //TODO REMOVE ME just for debug
            return ( -1);
        }

#ifdef VERBOSITY_DEBUG
    unsigned int num_zeros = 0;
    unsigned int num_special_words = 0;
    unsigned int num_zs_idle = 0;

    std::vector<uint64_t>::iterator it;
    for(it=layer_data->begin(); it<layer_data->end(); it++)
    {
        if((*it & 0xFFFFFFFFFFFFFFFF) == 0)
        {
            num_zeros++;
        }

        if(*it == 0x800000E700000001)
        {
            num_special_words++;
        }

        if((*it & 0x8000000000000000) != 0)
        {
            num_zs_idle++;
        }
    }
    log_utilities::debug("Control words in read data vector: %u num_zeros, %u num_special_words, %u num_zs_idle", num_zeros, num_special_words, num_zs_idle);
    //sleep(5); // TODO remove me, just for debug
#endif
    return (layer_data->size() * sizeof(uint64_t));

}

bool ZS_axidma::is_write_thread_running(void) {
    return (write_thread_running);
}

#endif

