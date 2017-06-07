#include "mex.h"
#include "matrix.h"

#include <vector>

#include "class_handle.hpp"

#include <FLAC++/decoder.h>

class BufferDecoder;

void getters(int nlhs, int nrhs, mxArray* plhs[],  const char* cmd, BufferDecoder* decoder);
void setters(int nlhs, int nrhs, const mxArray* prhs[], const char* cmd, BufferDecoder* decoder);
void initers(int nlhs, int nrhs, const mxArray* prhs[], const char* cmd, BufferDecoder* decoder);
void processors(int nlhs, int nrhs, mxArray* plhs[], const mxArray* prhs[], const char* cmd, BufferDecoder* decoder);
void seek_absolute(int nlhs, int nrhs, mxArray* plhs[], const mxArray* prhs[], BufferDecoder* decoder);
void is_valid(int nlhs, int nrhs, mxArray* plhs[], const mxArray* prhs[], BufferDecoder* decoder);

void buffer_ops(int nlhs, int nrhs, mxArray* plhs[], const mxArray* prhs[], const char* cmd, BufferDecoder* decoder);


class BufferDecoder: public FLAC::Decoder::File { 
    /* This class extends the FLAC::Decoder::File decoder so that it writes
     * data into a buffer (std::vector), which you can "export" to matlab on demand. 
     */
  
public:
    BufferDecoder() : FLAC::Decoder::File() { }
    
    void clear(void) { 
        buffer.clear(); 
    }
    
    void preallocate(size_t new_size) {
        buffer.reserve(new_size);
    }
    
    mxArray* to_mxArray(void) {
        /* Copy the buffer to an mxArray, which we then export to matlab.
         * I debated making this just export buffer.data() to avoid the copy
         * but that seems unnecessarily brittle.
         */
        unsigned n_channels =  this->get_channels(); 
        if (buffer.size() > 0) {
            mxArray *array = mxCreateUninitNumericMatrix(n_channels, buffer.size() / n_channels, mxINT32_CLASS, mxREAL);
            uint32_T* dst = static_cast<uint32_T*>(mxGetData(array));
            uint32_T* src = static_cast<uint32_T*>(buffer.data());
            memcpy(dst, src, buffer.size() * sizeof(buffer[0]));
            dst = src;
            return array;
        } else {
            // Return an empty array if the buffer is empty....
            mxArray *array = mxCreateNumericMatrix(n_channels, 0, mxINT32_CLASS, mxREAL);
            return array;
        }
    }
    
protected:
   std::vector<FLAC__uint32> buffer;     
   
   FLAC__StreamDecoderWriteStatus write_callback(const ::FLAC__Frame *frame, const FLAC__int32 * const buffer[]) {       
       
       /* This is incredibly cache-unfriendly. Sorry! */
       unsigned n_channels = frame->header.channels;
       auto first_sample = frame->header.number.sample_number; 
       auto last_sample  = frame->header.number.sample_number +  frame->header.blocksize - 1;
      
       for(auto i=0; i < frame->header.blocksize; i++) {               
           for(auto c=0; c < n_channels; c++) {
               this->buffer.push_back(buffer[c][i]);
           }
       }
       
       return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
   }
   
   void error_callback(FLAC__StreamDecoderErrorStatus status) {
       mexErrMsgIdAndTxt("FileDecoder:Internal:DecodeError", 
               FLAC__StreamDecoderErrorStatusString[status]);
       return;
   }
};




void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[]) {
    char cmd[64];
    if (nrhs < 1 || mxGetString(prhs[0], cmd, sizeof(cmd))) {
		mexErrMsgTxt("First input should be a command string less than 64 characters long.");
        return;
    }
    
     // New
    if (!strcmp("new", cmd)) {
        // Check parameters
        if (nlhs != 1)
            mexErrMsgTxt("New: One output expected.");
        // Return a handle to a new C++ instance
        plhs[0] = convertPtr2Mat<BufferDecoder>(new BufferDecoder);
        return;
    }
    
    if (nrhs < 2) {
		mexErrMsgTxt("Second input should be a class instance handle.");
    }
      
    // Delete
    if (!strcmp("delete", cmd)) {
        // Not sure how much of this actually needs to be done, but...
        BufferDecoder* encoder = convertMat2Ptr<BufferDecoder>(prhs[1]);
        bool ok = encoder->finish();
        if(!ok) {
            mexWarnMsgTxt("Unable to finalize decoder. Some data may have been lost."); 
        }
        
        destroyObject<BufferDecoder>(prhs[1]);
        // Warn if other commands were ignored
        if (nlhs != 0 || nrhs != 2)
            mexWarnMsgTxt("Delete: Unexpected arguments ignored.");
        return;       
    }

    BufferDecoder *decoder = convertMat2Ptr<BufferDecoder>(prhs[1]);
    if(!decoder)
        mexWarnMsgTxt("Something is broken");
                 
    if(!strncmp("get_", cmd, 4)) {
        getters(nlhs, nrhs, plhs, cmd, decoder);
    } else if(!strncmp("set_", cmd, 4)) {
        setters(nlhs, nrhs, prhs, cmd, decoder);
    } else if(!strncmp("init", cmd, 4)) {
        initers(nlhs, nrhs, prhs, cmd, decoder);     
    } else if(!strncmp("process_", cmd, 4)) {        
        processors(nlhs, nrhs, plhs, prhs, cmd, decoder);
    } else if(!strncmp("buffer", cmd, 6)) {
        buffer_ops(nlhs, nrhs, plhs, prhs, cmd, decoder);
    } else if(!strcmp("is_valid", cmd)) {
        bool is_valid = decoder->is_valid();
    } else if(!strcmp("seek_absolute", cmd)) {
        seek_absolute(nlhs, nrhs, plhs, prhs, decoder);
    }
    else {
        mexErrMsgIdAndTxt("FileEncoder:UnknownCommand", "Unknown command!");
    }    
    return;
}

void getters(int nlhs, int nrhs, mxArray* plhs[],  const char* cmd, BufferDecoder* decoder) {
    // Specialized getters
    if(!strcmp("get_state", cmd) || !strcmp("get_channel_assignment", cmd)) {
        if(nlhs > 2 || nrhs !=2) {
            mexErrMsgIdAndTxt("FileEncoder:Internal:SpecialGetArgs", 
                    "Special getter should have one or two output arguments, plus obj/command inputs");
        }
        
        if(!strcmp("get_state", cmd)) {
            int code = static_cast<int>(decoder->get_state());
            plhs[0] = mxCreateDoubleScalar(code);
            if(nlhs > 1) {
                plhs[1] = mxCreateString(FLAC__StreamDecoderStateString[code]);                        
            }
            return;
        } else if(!strcmp("get_channel_assignment", cmd)) {
            int code = static_cast<int>(decoder->get_channel_assignment());
            plhs[0] = mxCreateDoubleScalar(code);
            if(nlhs > 1) {
                plhs[1] = mxCreateString(FLAC__ChannelAssignmentString[code]);
            }
            return;
        }
    }
     
    //If you've gotten this far, we're in the regular getters
    if(nlhs > 1 || nrhs != 2) {
        mexErrMsgIdAndTxt("FileDecoder:Internal:GetArgs", 
                "Getter should have one output argument, plus obj/command inputs, but nlhs= %d and nrhs=%d.", nlhs, nrhs);
    }
    
    if(!strcmp("get_md5_checking", cmd)) {
        plhs[0] = mxCreateLogicalScalar(static_cast<bool>(decoder->get_md5_checking()));
    } else if(!strcmp("get_total_samples", cmd)) {
        plhs[0] = mxCreateNumericMatrix(1,1, mxUINT64_CLASS , mxREAL);
        *((uint64_T*)(mxGetData(plhs[0]))) = static_cast<uint64_T>(decoder->get_total_samples());
    } else if(!strcmp("get_channels", cmd)) {
        plhs[0] = mxCreateDoubleScalar(static_cast<double>(decoder->get_channels()));
    } else if(!strcmp("get_bits_per_sample", cmd)) {
        plhs[0] = mxCreateDoubleScalar(decoder->get_bits_per_sample());
    } else if(!strcmp("get_sample_rate", cmd)) {
        plhs[0] = mxCreateDoubleScalar(decoder->get_sample_rate());
    } else if(!strcmp("get_blocksize", cmd)) {
        plhs[0] = mxCreateDoubleScalar(decoder->get_blocksize());
    } else if(!strcmp("get_decode_position", cmd)) {
        FLAC__uint64 position;
        bool ok = decoder->get_decode_position(&position);
        if(!ok)
            mexErrMsgIdAndTxt("FileDecoder:Internal:NoPosition", 
              "Could not recover decoder position (see docs for reasons)");
        plhs[0] = mxCreateNumericMatrix(1, 1, mxUINT64_CLASS, mxREAL);
        *((uint64_T*)(mxGetData(plhs[0]))) = static_cast<uint64_T>(position);        
    } else {
        mexErrMsgIdAndTxt("FileDecoder:Internal:GetNotImplemented", 
                "No getter implemented for %s", cmd);
    }
}

void setters(int nlhs, int nrhs, const mxArray* prhs[], const char* cmd, BufferDecoder*decoder) {
    /* Setters for the FileDecoder (these are not very interesting for decoding):
     - set_ogg_serial_number
     - set_md5_checking
     */
    
    if(nlhs > 0 || nrhs != 3) {
        mexErrMsgIdAndTxt("FileDecoder:Internal:SetArgs", 
                "Setter should have no output arguments, plus obj/command input and a scalar value, but nlhs= %d and nrhs=%d.", nlhs, nrhs);
     }
     
     bool ok =  false;
     if(!strcmp("set_ogg_serial_number", cmd)) {
         ok = decoder->set_ogg_serial_number(static_cast<long>(mxGetScalar(prhs[2])));
         if(!ok)
             mexErrMsgIdAndTxt("FileDecoder:Internal:SerialNumberSet",
                "Could not set OGG serial number", cmd);
     } else if(!strcmp("set_md5_checking", cmd)) {
         ok = decoder->set_md5_checking(static_cast<bool>(mxGetScalar(prhs[2])));
          if(!ok) {
             mexErrMsgIdAndTxt("FileDecoder:Internal:MD5Set",
                "Could not set md5 checking", cmd);
          }
     }
}

void initers(int nlhs, int nrhs, const mxArray* prhs[], const char* cmd, BufferDecoder* decoder) {
    /* Handles the initializers (init, init_ogg) */
    if(nlhs > 0 || nrhs !=3) {
        mexErrMsgIdAndTxt("FileDecoder:Internal:InitArgs", 
                "Initers take one argument (filename) and returns nothing", nlhs, nrhs);
     } 

    const char* filename = mxArrayToString(prhs[2]);
    if(!filename)
         mexErrMsgIdAndTxt("FileDecoder:Internal:InitArgs", 
                "Filename cannot be converted to a string");
    
    FLAC__StreamDecoderInitStatus status;
    if(!strcmp("init", cmd)) {
        status = decoder->init(filename);
    } else if(!strcmp("init_ogg", cmd)) {
        status = decoder->init_ogg(filename);
    } else {
        mexErrMsgIdAndTxt("FileDecoder:UnknownCommand", "Command not recognized");
    }
    
    switch(status) {
        case FLAC__STREAM_DECODER_INIT_STATUS_OK:
            return;
            break;
            
        case FLAC__STREAM_DECODER_INIT_STATUS_UNSUPPORTED_CONTAINER:
            mexErrMsgIdAndTxt("FileDecoder:UnsupportedContainer",
                              "Library not compiled with support for the given container format (ogg?)");
            break;
              
        case FLAC__STREAM_DECODER_INIT_STATUS_INVALID_CALLBACKS:           
            //Not sure if this can actually happen for File::Encoder
            mexErrMsgIdAndTxt("FileDecoder:InvalidCallbacks", "A required callback was not supplied");
            break;
              
        case FLAC__STREAM_DECODER_INIT_STATUS_MEMORY_ALLOCATION_ERROR:
            // Pretty vague, eh?
            mexErrMsgIdAndTxt("FileDecoder:MemoryError", "An error occurred allocating memory.");
            break;

        case FLAC__STREAM_DECODER_INIT_STATUS_ERROR_OPENING_FILE:
            // Not sure what corresponding error is for encoder
            mexErrMsgIdAndTxt("FileDecoder:FileError", "Unable to open file");
            break;
            
        case FLAC__STREAM_DECODER_INIT_STATUS_ALREADY_INITIALIZED:
            mexErrMsgIdAndTxt("FileEncoder:AlreadyInit", 
                    "init() called when encoder was already initialized. Did you forgot to call finish()");
            break;
        default:
             mexErrMsgIdAndTxt("FileEncoder:Unknown", "Unknown error! Please file a bug report!");
             break;
    }
    return;
}

void processors(int nlhs, int nrhs, mxArray* plhs[], const mxArray* prhs[], const char* cmd, BufferDecoder* decoder) {
    /* Process FLAC file data:
      - process_until_end_of_metadata: Process (well, skip for now) the metadata
      - process_single: Process a single FLAC frame
      - process_until_end_of_stream: Process until the end of file
    */
    
    if(nlhs !=1 || nrhs !=2) {
        mexErrMsgIdAndTxt("FileDecoder:Internal:ProcessArgs", 
                "Processors take no arguments and returns one scalar", nlhs, nrhs);
     } 
    
    bool ok;
    if(!strcmp(cmd, "process_single")) {
        ok = decoder->process_single();        
    } else if(!strcmp(cmd, "process_until_end_of_metadata")) {
       ok = decoder->process_until_end_of_metadata();                
    }  else if(!strcmp(cmd, "process_until_end_of_stream")) {
        ok = decoder->process_until_end_of_stream();
    }
    
    plhs[0] = mxCreateLogicalScalar(ok);
    return;            
}


void buffer_ops(int nlhs, int nrhs, mxArray* plhs[], const mxArray* prhs[], const char* cmd, BufferDecoder* decoder) {
    /* Manage the BufferDecoder's internal buffer:
     - buffer_preallocate: Preallocate the BufferDecoder's internal buffer
     - buffer_clear: Clear internal buffer
     - buffer_to_matlab: Transfer the internal buffer to matlab
    */
     
    if(!strcmp(cmd, "buffer_to_matlab")) {
        if(nlhs != 1 || nrhs != 2) {
            mexErrMsgIdAndTxt("FileDecoder:Internal:BufferToMatlab",
                    "Function takes no arguments and returns one matrix", nlhs, nrhs);
        }

        plhs[0] = decoder->to_mxArray();
        
    } else if(!strcmp(cmd, "buffer_clear")) {
        if(nlhs > 0 || nrhs != 2) {
            mexErrMsgIdAndTxt("FileDecoder:Internal:BufferClearArgs",
                    "Function take no arguments and returns nothing", nlhs, nrhs);
          }
          decoder->clear();
    
    } else if(!strcmp(cmd, "buffer_preallocate")) {
        if(nlhs > 0 || nrhs != 3) {
            mexErrMsgIdAndTxt("FileDecoder:Internal:BufferPreallocateArgs",
                    "Function takes one scalar argument and returns nothing", nlhs, nrhs);
        }
        decoder->preallocate(static_cast<size_t>(mxGetScalar(prhs[2])));
    
    } else if(!strcmp(cmd, "buffer_length")) {
        if(nlhs > 0 || nrhs !=3) {
            mexErrMsgIdAndTxt("FileDecoder:Internal:BufferClearArgs",
                    "Function takes no arguments and returns a scalar", nlhs, nrhs);
        }
    }
            
}

void seek_absolute(int nlhs, int nrhs, mxArray* plhs[], const mxArray* prhs[], BufferDecoder* decoder) {
    if(nlhs > 1 || nrhs != 3) {
        mexErrMsgIdAndTxt("FileDecoder:Internal:SeekArgs", 
             "seek_absolute takes one argument (plus obj/command inputs), but nlhs= %d and nrhs=%d.", nlhs, nrhs);
    }
    
    plhs[0] = mxCreateLogicalScalar(static_cast<bool>(decoder->seek_absolute(static_cast<long>(mxGetScalar(prhs[2])))));
}

void is_valid(int nlhs, int nrhs, mxArray* plhs[], const mxArray* prhs[], BufferDecoder* decoder) {

    if(nlhs > 1 || nrhs != 2) {
        mexErrMsgIdAndTxt("FileDecoder:Internal:ValidArgs",
                "is_valid takes no arguments (other than obj/command inputs), but nlhs= %d and nrhs=%d.", nlhs, nrhs);
    }
    
    plhs[0] = mxCreateLogicalScalar(static_cast<bool>(decoder->is_valid()));
}