/*
 *	Low level functions to control the AXIDMA device
 *	author: Antonio Rios (arios@atc.us.es)
 */

#ifndef __AXIDMA_H__
#define __AXIDMA_H__

#include <fcntl.h>
#include <sys/mman.h>
//#include <curses.h>
#include <cstring>
#include <vector>
#include <unistd.h>
#include <math.h>
#include <chrono>
#include <exception>
#include <ctime>
#include <ratio>
#include <stdexcept>
#include "axi_channel_timeout_excep.cpp"
#include "npp_log_utilities.cpp"

#if defined (AXIDMA_TIMING)
#include <stdio.h>
#endif

#define AXIDMA_MEMORY_MAPPING_WRITE_SIZE_DEFINE 0x800000 	//8MB
#define AXIDMA_MEMORY_MAPPING_READ_SIZE_DEFINE 0x800000		//8MB

/** \brief Axidma Abstract class. Low level axi dma class controller.
 *
 * This class manages the registers of AXIDMA controller that are mapped on memory. There are functions to configure both axidma
 * channels (MM2S and S2MM) and to program the transfers.
 */
class Axidma {
    protected:
        const unsigned int MM2S_CONTROL_REGISTER; //!< Register offset that provides control for the Memory Map to Stream DMA channel
        const unsigned int MM2S_STATUS_REGISTER; //!< Register offset that provides the status for the Memory Map to Stream DMA channel
        const unsigned int MM2S_START_ADDRESS; //!< Register offset that provides the source address for reading system memory for the Memory Map to Stream DMA transfer
        const unsigned int MM2S_LENGTH; //!< Register offset that provides the number of bytes to read from the system memory and transfer to MM2S AXI4-Stream

        const unsigned int S2MM_CONTROL_REGISTER; //!< Register offset that provides control for the Stream to Memory Map DMA channel
        const unsigned int S2MM_STATUS_REGISTER; //!< Register offset that provides the status for the Stream to Memory Map DMA channel
        const unsigned int S2MM_DESTINATION_ADDRESS; //!< Register offset that provides the destination address for writing to system memory for the Stream Memory Map DMA transfer
        const unsigned int S2MM_LENGTH; //!< Register offset that provides the length in bytes of the write data from the S2MM DMA transfer

        const unsigned int RUNNING;                 //!< DMA channel status that indicates the channel is ready to new transfer
        const unsigned int HALTED;                  //!< DMA channel status that indicates the channel is stopped
        const unsigned int IDLE;  //!< DMA channel status that indicates a DMA tranfer has been completed and controller is paused
        const unsigned int SGINCLD;                 //!< DMA channel status that indicates if Scatter Gather is enable or not
        const unsigned int IOC_IRQ; //!< DMA channel status for interrupt on complete. Indicates an interrupt event was generated on completion of a transfer.
        const unsigned int DLY_IRQ; //!< DMA channel status for interrupt on delay. Indicates an interrupt event was generated on delay timer interrupt.
        const unsigned int ERR_IRQ; //!< DMA channel status for interrupt on error. Indicates an interrupt event was generated on error.

        const unsigned int AXIDMA_ADDR_OFFSET;     //!< Memory offset that points where hardware AXIDMA module is mapped on memory
        const unsigned int DESTINATION_ADDR_OFFSET; //!< Memory offset that points to reserved memory space for S2MM data
        const unsigned int SOURCE_ADDR_OFFSET;      //!< Memory offset that points to reserved memory space for MM2S data
        unsigned int SOURCE_ADDR_OFFSET2;
        unsigned int DESTINATION_ADDR_OFFSET2;
        const unsigned int MAX_WRITE_TRANSFER_LENGTH_BYTES;
        const unsigned int MAX_READ_TRANSFER_LENGTH_BYTES;
        const unsigned int MIN_READ_TRANSFER_LENGTH_BYTES;

        int whole_memory_pointer;
        unsigned int* axidma_map_addr;
        uint64_t* source_addr;
        uint64_t* source_addr2;
        uint64_t* destination_addr;
        uint64_t* destination_addr2;
        uint64_t operating_mode_partial, operating_mode_complete;
        unsigned int read_transfer_length_bytes;
        unsigned int axidma_channel_timeout_us;
        unsigned int num_word_per_max_size_write;

        unsigned int num_word_per_read;


        AXIDMA_timeout_exception axi_channel_timeout_excep;

        typedef std::chrono::duration<int, std::ratio<1, 1000000>> time_span_us; //!< Microsecond definition for AXIDMA channel timeout synchronization

        /** \brief Set AXIDMA register value
         *
         *  @param register_offset register to change its value
         *  @param value new register value
         */
        void set_dma_register_value(int register_offset, unsigned int value);

        /** \brief Get AXIDMA register value
         *
         *  @param register_offset register to change its value
         *  @return value of the selected AXIDMA register
         */
        unsigned int get_dma_register_value(int register_offset);

        /** \brief Synchronization for MM2S channel
         *
         *  This synchronization has a timeout to avoid a infinity sync
         */
        virtual void mm2s_sync(void) throw (AXIDMA_timeout_exception) = 0;

        /** \brief Synchronization for S2MM channel
         *
         *  This synchronization has a timeout to avoid a infinity sync
         */
        virtual void s2mm_sync(void) throw (AXIDMA_timeout_exception) = 0;

#if defined (AXIDMA_TIMING)
        FILE* axidma_write_timing_file;
        FILE* axidma_read_timing_file;
#endif

    public:
        const unsigned int DMAINTERR; //!< DMA channel status (DMA internal error). This error occurs if the buffer length specified does not fix with the transfer length.
        const unsigned int DMASLVERR; //!< DMA channel status (DMA slave error). This error occurs if the slave read from the Memory map interface issues a Slave Error
        const unsigned int DMADECERR; //!< DMA channel status (DMA decode error). This error occurs if the address request points to an invalid address
        const unsigned int SGINTERR; //!< DMA channel status (Scatter Gather internal error). This error occurs if a descriptor with the "Complete bit" already set is fetched
        const unsigned int SGSLVERR; //!< DMA channel status (Scatter Gather slave error). This error occurs if the slave read from on Memory Map interface issues a Slave error
        const unsigned int SGDECERR; //!< DMA channel status (Scatter Gather decode error). This error occurs if the CURDESC_PTR and/or NXTDESC_PTR points to an invalid address

        /** \brief Constructor
         *
         *  @param axidma_addr_offset set the memory pointer where the AXIDMA module is mapped
         *  @param source_addr_offset set the memory pointer where data are read from MM2S channel
         *  @param destination_addr_offset the memory pointer where data are written by S2MM channel
         */
        Axidma(unsigned int axidma_addr_offset, unsigned int source_addr_offset, unsigned int destination_addr_offset);

        /** \brief Destructor
         */
        virtual ~Axidma(void) = 0;

        /** \brief AXIDMA channels initialization
         *
         *  @param trans_read_length_bytes set the S2MM transfer length in bytes
         */
        bool init(unsigned int trans_read_length_bytes);

        /** \brief Print MM2S channel status that is coded in MM2S_STATUS_REGISTER
         */
        void print_mm2s_status(void);

        /** \brief Print S2MM channel status that is coded in S2MM_STATUS_REGISTER
         */
        void print_s2mm_status(void);

        /** \brief Check if MM2S channel is under a specified status
         *
         *  @param chk_status status to be checked
         */
        bool check_mm2s_status(unsigned int chk_status);

        /** \brief Check if S2MM channel is under a specified status
         *
         *  @param chk_status status to be checked
         */
        bool check_s2mm_status(unsigned int chk_status);

        /** \brief Reset both AXIDMA channels
         */
        void reset(void);

        /** \brief Stop both AXIDMA channels
         */
        void stop(void);

        /** \brief Read data that S2MM channel has written
         *
         *  @param data vector pointer to read data will be copied from S2MM channel
         *  @return number of read bytes from the actual S2MM tranfer
         */
        unsigned int read(std::vector<uint64_t> *data);

        /** \brief Write data to MM2S channel
         *
         *  @param data vector that will be write to MM2S channel
         *  @return number of written bytes from the actual MM2S tranfer
         */
        unsigned int write(std::vector<uint64_t> *data);
        void write_splitted(const std::vector<uint64_t> * data);

        /** \brief Clear MM2S transfer flags to indicate that transfer synchronization has been detected
         */
        void clear_mm2s_flags(void);

        /** \brief Clear S2MM transfer flags to indicate that transfer synchronization has been detected
         */
        void clear_s2mm_flags(void);

        /** \brief Get the timeout value specified to avoid synchronizations forever
         *
         *  @return sync AXIDMA channel timeout in microseconds
         */
        unsigned int get_axidma_channel_timeout(void);

        /** \brief Set the timeout value specified to avoid synchronizations forever
         *
         *  @param value sync AXIDMA channel timeout in microseconds
         */
        void set_axidma_channel_timeout(unsigned int value);

        /** \brief Get the S2MM transfer length in bytes
         *
         *  @return number of bytes for S2MM transfer length
         */
        unsigned int get_read_transfer_length_bytes(void);
};

/** \brief Axidma_pool class. Low level axi dma class controller using pooling sync.
 *
 * This class manages the registers of AXIDMA controller that are mapped on memory. There are functions to configure both axidma
 * channels (MM2S and S2MM) and to program the transfers.
 */
class Axidma_pool: public Axidma {
    private:
        /** \brief Pooling synchronization for MM2S channel
         *
         *  This synchronization has a timeout to avoid a infinity pooling sync
         */
        void mm2s_sync(void) throw (AXIDMA_timeout_exception);

        /** \brief Pooling synchronization for S2MM channel
         *
         *  This synchronization has a timeout to avoid a infinity pooling sync
         */
        void s2mm_sync(void) throw (AXIDMA_timeout_exception);

    public:
        /** \brief Constructor
         *
         *  @param axidma_addr_offset set the memory pointer where the AXIDMA module is mapped
         *  @param source_addr_offset set the memory pointer where data are read from MM2S channel
         *  @param destination_addr_offset the memory pointer where data are written by S2MM channel
         */
        Axidma_pool(unsigned int axidma_addr_offset, unsigned int source_addr_offset, unsigned int destination_addr_offset);
};

/** \brief Axidma_int class. Low level axi dma class controller using interruption sync.
 *
 * This class manages the registers of AXIDMA controller that are mapped on memory. There are functions to configure both axidma
 * channels (MM2S and S2MM) and to program the transfers.
 */
class Axidma_int: public Axidma {
    private:
        int IRQ_CHK;
        int IRQ_CLR;

        int mm2s_irq;
        int s2mm_irq;

        fd_set rfd_s2mm, rfd_mm2s;

        struct timeval mm2s_timeout;
        struct timeval s2mm_timeout;

        /** \brief Interruption synchronization for MM2S channel
         *
         *  This synchronization has a timeout to avoid a infinity interruption sync
         */
        void mm2s_sync(void) throw (AXIDMA_timeout_exception);

        /** \brief Interruption synchronization for S2MM channel
         *
         *  This synchronization has a timeout to avoid a infinity interruption sync
         */
        void s2mm_sync(void) throw (AXIDMA_timeout_exception);

    public:
        /** \brief Constructor
         *
         *  @param axidma_addr_offset set the memory pointer where the AXIDMA module is mapped
         *  @param source_addr_offset set the memory pointer where data are read from MM2S channel
         *  @param destination_addr_offset the memory pointer where data are written by S2MM channel
         */
        Axidma_int(unsigned int axidma_addr_offset, unsigned int source_addr_offset, unsigned int destination_addr_offset);
};

#endif
