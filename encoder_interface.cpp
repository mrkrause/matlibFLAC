#include "mex.h"
#include "matrix.h"

#include "class_handle.hpp"

#include <FLAC++/encoder.h>

void generic_getters(int nlhs, mxArray *plhs[], int nrhs, const char* cmd, FLAC::Encoder::File *encoder);
void generic_setters(int nlhs, int nrhs, const mxArray *plhs[], const char* cmd, FLAC::Encoder::File *encoder);
void get_verify_decoder_error_stats(int lhs, mxArray* plhs[], int nrhs, FLAC::Encoder::File* encoder);             

void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray *prhs[]) {	
    // Get the command string
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
        plhs[0] = convertPtr2Mat<FLAC::Encoder::File>(new FLAC::Encoder::File);
        return;
    }
    
    if (nrhs < 2)
		mexErrMsgTxt("Second input should be a class instance handle.");
    
    // Delete
    if (!strcmp("delete", cmd)) {
        // Not sure how much of this actually needs to be done, but...
        FLAC::Encoder::File* encoder = convertMat2Ptr<FLAC::Encoder::File>(prhs[1]);
        bool ok = encoder->finish();
        if(!ok) {
            mexWarnMsgTxt("Something happened"); //TODO: Real error message
        }
                
        destroyObject<FLAC::Encoder::File>(prhs[1]);
        // Warn if other commands were ignored
        if (nlhs != 0 || nrhs != 2)
            mexWarnMsgTxt("Delete: Unexpected arguments ignored.");
        return;       
    }

    // Get the class instance pointer from the second input
    FLAC::Encoder::File *encoder = convertMat2Ptr<FLAC::Encoder::File>(prhs[1]);
  
    /* Process all commands beginning with "get_". 
        The really trivial one-liners are all in getters(), but the 
        get_state/error functions are a little less trivial since we may
        want to return multiple arguments. Thus, they get their own functions.
     */    
    if(!strncmp("get_", cmd, 3)) {
        if(!strcmp("get_state", cmd)) {
            if(nlhs > 2 || nrhs > 2) {
                mexErrMsgIdAndTxt("FileEncoder:Internal:GetArgs", "Special getter may only have 1 or 2 output args and no input args");
            }
            
            FLAC::Encoder::Stream::State state = encoder->get_state();
            plhs[0] = mxCreateDoubleScalar(state);
            if(nlhs > 1) {
                // I *think* these strings are static and thus don't need to be freed
                plhs[1] = mxCreateString(state.as_cstring());                
            }                        
        } else if(!strcmp("get_verify_decoder_state", cmd)) {
            if(nlhs > 2 || nrhs > 2) {
                mexErrMsgIdAndTxt("FileEncoder:Internal:GetArgs", "Special getter may only have 1 or 2 output args and no input args");
            }
            
            FLAC::Decoder::Stream::State state = encoder->get_verify_decoder_state();
            plhs[0] = mxCreateDoubleScalar(0);
            if(nlhs > 1) {
                // I *think* these strings are static and thus don't need to be freed
                plhs[1] = mxCreateString(state.as_cstring());                
            }                       
        } else if(!strcmp("get_verify_decoder_error_stats", cmd)) {
            get_verify_decoder_error_stats(nlhs, plhs, nrhs, encoder);            
        } else {            
            generic_getters(nlhs, plhs, nrhs, cmd, encoder);            
        }       
        return; //Errors caught by getters   
        
    /*Setters here. The meta stuff is complicated so it gets its own function. */
    } else if(!strncmp("set_", cmd, 3)) {
        if(!strcmp("set_metadata", cmd)) {
           //Some stuff here
        } else {
            generic_setters(nlhs, nrhs, prhs, cmd, encoder);            
        }
        return;
    } 
    /* Everything else (init, process, finish) */
    else {
        if(!strcmp("init", cmd) || !strcmp("init_ogg", cmd)) {            
            char* filename = mxArrayToString(prhs[2]);
            if(!filename) {
                mexErrMsgIdAndTxt("FileEncoder:FilenameNotString", "Filename is not a string or convertible to one.");            
            }
            
            int status;            
            if(!strcmp("init", cmd))
                status = encoder->init(filename);
            else
                status = encoder->init_ogg(filename);
         
            switch(status) {
                // These codes are all taken from the docs
                case FLAC__STREAM_ENCODER_INIT_STATUS_OK:
                    return;
                    break;
                case FLAC__STREAM_ENCODER_INIT_STATUS_ENCODER_ERROR: 	
                    mexErrMsgIdAndTxt("FileEncoder:EncoderSetup", "Failed to set up encoder (call get_state for details)");
                    break;

                case FLAC__STREAM_ENCODER_INIT_STATUS_UNSUPPORTED_CONTAINER:
                    mexErrMsgIdAndTxt("FileEncoder:UnsupportedContainer", "Library not compiled with support for the given container format (ogg?)");
                    break;
                
                case FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_CALLBACKS:
                    //Not sure if this can actually happen for File::Encoder
                    mexErrMsgIdAndTxt("FileEncoder:InvalidCallbacks", "A required callback was not supplied");
                    break;
                    
                case FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_NUMBER_OF_CHANNELS:
                    mexErrMsgIdAndTxt("FileEncoder:InvalidChannels", "Invalid setting for number of channels");
                    break;
                    
                case FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_BITS_PER_SAMPLE:
                    mexErrMsgIdAndTxt("FileEncoder:InvalidBitsPerSample", "Invalid setting for bits per sample");
                    break;
                    
                case FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_SAMPLE_RATE:
                    mexErrMsgIdAndTxt("FileEncoder:InvalidSampleRate", "Invalid setting for input sample rate");
                    break;
                    
                case FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_BLOCK_SIZE:
                    mexErrMsgIdAndTxt("FileEncoder:InvalidBlockSize", "Invalids setting for the block size");
                    break;
                
                case FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_MAX_LPC_ORDER:
                    mexErrMsgIdAndTxt("FileEncoder:InvalidMaxLPC", "Invalid setting for maximum LPC order");
                    break;
                    
                case FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_QLP_COEFF_PRECISION:
                    mexErrMsgIdAndTxt("FileEncoder:InvalidQLPCoeffPrecision", "Invalid setting for QLP coefficient precision");
                    break;
                
                case FLAC__STREAM_ENCODER_INIT_STATUS_BLOCK_SIZE_TOO_SMALL_FOR_LPC_ORDER:
                    mexErrMsgIdAndTxt("FileEncoder:BlockTooSmall", "Block size is less than the maximum LPC order");
                    break;
                    
                case FLAC__STREAM_ENCODER_INIT_STATUS_NOT_STREAMABLE:
                    mexErrMsgIdAndTxt("FileEncoder:NotStreamable", "Encoder was configured for streamable subset, but other settings violate this request.");
                    break;
                    
                case FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_METADATA:
                    mexErrMsgIdAndTxt("FileEncoder:InvalidMetaData", "Metadata is invalid; see libFLAC docs for possibilities");
                    break;
                    
                case FLAC__STREAM_ENCODER_INIT_STATUS_ALREADY_INITIALIZED:
                    mexErrMsgIdAndTxt("FileEncoder:AlreadyInit", "init() called when encoder was already initialized. Did you forgot to call finish()");
                    break;
                    
                default:
                    mexErrMsgIdAndTxt("FileEncoder:Unknown", "Unknown error! Please file a bug report!");
                    break;
            }
            return;
            
        } else if(!strcmp("process", cmd)) {
            if(nlhs > 1 || nrhs != (encoder->get_channels()+2)) {
                mexErrMsgIdAndTxt("FileEncoder:Process:ArgCount", "Wrong number of arguments. Process takes one array per channel");
            }
            
            for(int i=3; i<nrhs; i++)
                if(!(mxIsInt32(prhs[i]) && mxGetNumberOfDimensions(prhs[i])==2 && (mxGetM(prhs[i]) == 1 || mxGetN(prhs[i]) == 1)))
                    mexErrMsgIdAndTxt("FileEncoder:Process:ArgType", "All arguments to process must be signed int32 vectors");
            
            
            
            FLAC__int32** buffer = new FLAC__int32*[1];
            
            for(int i=3; i<nrhs; i++)
                buffer[i] = static_cast<FLAC__int32*>(mxGetData(prhs[i]));
            bool ok = encoder->process(buffer, std::max(mxGetM(prhs[3]),mxGetN(prhs[3])));
            
        } else if(!strcmp("process_interleaved", cmd)) {
            if(nlhs > 1 || nrhs != 3) {
                mexErrMsgIdAndTxt("FileEncoder:Process:ArgCount", "Wrong number of arguments. Process takes one array per channel");
            }
            
            if(!mxIsInt32(prhs[2])) {
                mexErrMsgIdAndTxt("FileEncoder:Process:ArgType", "Data argument to process_interleaved must be a signed int32 matrix");
            }
            
            bool ok = encoder->process_interleaved(static_cast<FLAC__int32*>(mxGetData(prhs[2])), std::max(mxGetM(prhs[2]),mxGetN(prhs[2])));
            plhs[0] = mxCreateLogicalScalar(ok);
            return;
        } else if(!strcmp("finish", cmd)) {
            bool ok = encoder->finish();
            plhs[0] = mxCreateLogicalScalar(ok);
            return;
        }

            
            
    }                                               
    mexErrMsgTxt(cmd);
}

void generic_getters(int nlhs, mxArray *plhs[], int nrhs, const char* cmd, FLAC::Encoder::File *encoder) {
    if(nlhs > 1 || nrhs != 2) {
        mexErrMsgIdAndTxt("FileEncoder:Internal:GetArgs", 
                "Getter should have one output argument, plus obj/command inputs, but nlhs= %d and nrhs=%d.", nlhs, nrhs);
    }
    /* A smart person might have made this O(1) with a hashmap or something. */
    if (!strcmp("get_verify", cmd)) {                   
        plhs[0] = mxCreateLogicalScalar(static_cast<bool>(encoder->get_verify()));               
    } else if(!strcmp("get_streamable_subset", cmd)) {
        plhs[0] = mxCreateLogicalScalar(static_cast<bool>(encoder->get_streamable_subset())); 
    } else if(!strcmp("get_do_mid_side_stereo", cmd)) {
         plhs[0] = mxCreateLogicalScalar(static_cast<bool>(encoder->get_do_mid_side_stereo()));
    } else if(!strcmp("get_loose_mid_side_stereo", cmd)) {
        plhs[0] = mxCreateLogicalScalar(static_cast<bool>(encoder->get_loose_mid_side_stereo()));
    } else if(!strcmp("get_channels", cmd)) {
        plhs[0] = mxCreateDoubleScalar(static_cast<double>(encoder->get_channels()));
    } else if(!strcmp("get_bits_per_sample", cmd)) {
        plhs[0] = mxCreateDoubleScalar(static_cast<double>(encoder->get_bits_per_sample()));   
    } else if(!strcmp("get_sample_rate", cmd)) {
        plhs[0] = mxCreateDoubleScalar(static_cast<double>(encoder->get_sample_rate()));  
    } else if(!strcmp("get_blocksize", cmd)) {
        plhs[0] = mxCreateDoubleScalar(static_cast<double>(encoder->get_blocksize()));
    } else if(!strcmp("get_max_lpc_order", cmd)) {
        plhs[0] = mxCreateDoubleScalar(static_cast<double>(encoder->get_max_lpc_order()));
    } else if(!strcmp("get_qlp_coeff_precision", cmd)) {
        plhs[0] = mxCreateDoubleScalar(static_cast<double>(encoder->get_qlp_coeff_precision()));
    } else if(!strcmp("get_do_qlp_coeff_prec_search", cmd)) {
        plhs[0] = mxCreateLogicalScalar(static_cast<bool>(encoder->get_do_qlp_coeff_prec_search()));
    } else if(!strcmp("get_do_exhaustive_model_search", cmd)) {
        plhs[0] = mxCreateLogicalScalar(static_cast<bool>(encoder->get_do_exhaustive_model_search()));
    } else if(!strcmp("get_min_residual_partition_order", cmd)) {
        plhs[0] = mxCreateDoubleScalar(static_cast<double>(encoder->get_min_residual_partition_order()));
    } else if(!strcmp("get_max_residual_partition_order", cmd)) {
        plhs[0] = mxCreateDoubleScalar(static_cast<double>(encoder->get_max_residual_partition_order()));
    } else if(!strcmp("get_total_samples_estimate", cmd)) {
        plhs[0] = mxCreateDoubleScalar(static_cast<double>(encoder->get_total_samples_estimate()));
    } else {
        mexErrMsgIdAndTxt("FileEncoder:Internals:NotImplemented", "Getter for %s is not implemented", cmd);
    }
}


 void generic_setters(int nlhs, int nrhs, const mxArray *prhs[], const char* cmd, FLAC::Encoder::File *encoder) {
    if(nlhs > 0 || nrhs != 3) {
        mexErrMsgIdAndTxt("FileEncoder:Internal:SetArgs", 
                "Setter should have no output arguments, plus 3 inputs (command, object, new value), but nlhs= %d and nrhs=%d.", nlhs, nrhs);
    }
    
    bool outcome = false;
    double value = mxGetScalar(prhs[2]);
    if(!strcmp("set_ogg_serial_number", cmd)) {
        outcome = encoder->set_ogg_serial_number(static_cast<long>(value));
    } else if (!strcmp("set_verify", cmd)) {                   
        outcome = encoder->set_verify(static_cast<bool>(value));
    } else if(!strcmp("set_streamable_subset", cmd)) {
        outcome = encoder->set_streamable_subset(static_cast<bool>(value));
    } else if(!strcmp("set_channels", cmd)) {
        outcome = encoder->set_channels(static_cast<unsigned>(value));
    } else if(!strcmp("set_bits_per_sample", cmd)) {
        outcome = encoder->set_bits_per_sample(static_cast<unsigned>(value));
    } else if(!strcmp("set_sample_rate", cmd)) {
        outcome = encoder->set_sample_rate(static_cast<unsigned>(value));
    } else if(!strcmp("set_compression_level", cmd)) {
        outcome = encoder->set_compression_level(static_cast<unsigned>(value));
    } else if(!strcmp("set_blocksize", cmd)) {
        outcome = encoder->set_blocksize(static_cast<unsigned>(value));
    } else if(!strcmp("set_mid_side_stereo", cmd)) {
        outcome = encoder->set_do_mid_side_stereo(static_cast<bool>(value));
    } else if(!strcmp("set_loose_mid_side_stereo", cmd)) {
        outcome = encoder->set_loose_mid_side_stereo(static_cast<bool>(value));
    } else if(!strcmp("set_apodization", cmd)) {
        char* str = mxArrayToString(prhs[2]);
        outcome = encoder->set_apodization(str);
        mxFree(str);
    } else if(!strcmp("set_max_lpc_order", cmd)) {
        outcome = encoder->set_max_lpc_order(static_cast<unsigned>(value));
    } else if(!strcmp("set_qlp_coeff_precision", cmd)) {
        outcome = encoder->set_qlp_coeff_precision(static_cast<unsigned>(value));
    } else if(!strcmp("set_do_qlp_coeff_prec_search", cmd)) {
        outcome = encoder->set_do_qlp_coeff_prec_search(static_cast<bool>(value));
    } else if(!strcmp("set_do_exhaustive_model_search", cmd)) {
        outcome = encoder->set_do_exhaustive_model_search(static_cast<bool>(value));
    } else if(!strcmp("set_min_residual_partition_order", cmd)) {
        outcome = encoder->set_min_residual_partition_order(static_cast<unsigned>(value));
    } else if(!strcmp("set_max_residual_partition_order", cmd)) {
        outcome = encoder->set_max_residual_partition_order(static_cast<unsigned>(value));
    } else if(!strcmp("set_total_samples_estimate", cmd)) {
        outcome = encoder->set_total_samples_estimate(static_cast<FLAC__uint64>(value));
    } else {
        mexErrMsgIdAndTxt("FileEncoder:Internal:SetUnknown', 'Setter for %s unknown or not implemented", cmd);
    }   
    
    if(!outcome) {
        mexErrMsgIdAndTxt("FileEncoder:Interal:SetFailed", "Could not set %s.", cmd);
    }
}
        
void get_verify_decoder_error_stats(int nlhs, mxArray* plhs[], int nrhs, FLAC::Encoder::File* encoder) {                
    if(nlhs > 1 || nrhs !=2) {
        mexErrMsgIdAndTxt("FileEncoder:Internal:GetArgs",  "Getter should have one output argument, plus obj/command inputs");
    }
            
    //Layout of the struct we're going to return
    static const char* fieldnames[] = {"absolute_sample", "frame", "channel", "sample", "expected", "got"};
    const int n_fields = 6;
    const mwSize n_dims = 2;
    static const mwSize struct_dims[2] = { static_cast<mwSize>(1), static_cast<mwSize>(1)};         
    
    // Get the error state
    FLAC__uint64 absolute_sample;
    unsigned frame_number, channel, sample;            
    FLAC__int32 expected, got;
            
    encoder->get_verify_decoder_error_stats(&absolute_sample,
                    &frame_number, &channel, &sample, &expected, &got);
    
    // Package it up as a struct
    plhs[0] = mxCreateStructArray(n_dims, struct_dims, n_fields, fieldnames);
    
    mxArray* tmp = mxCreateNumericMatrix(1,1, mxUINT64_CLASS , mxREAL);
    *((uint64_T*)(mxGetData(tmp))) = static_cast<uint64_T>(absolute_sample);
    mxSetFieldByNumber(plhs[0], 0, 0, tmp);
        
    tmp = mxCreateNumericMatrix(1,1, mxUINT64_CLASS , mxREAL);
    *((uint64_T*)(mxGetData(tmp))) = static_cast<uint64_T>(frame_number);
    mxSetFieldByNumber(plhs[0], 0, 1, tmp);
    
    // The others should probably remain uint64/uint32
    mxSetFieldByNumber(plhs[0], 0, 2, mxCreateDoubleScalar(static_cast<double>(channel)));
    
    tmp = mxCreateNumericMatrix(1,1, mxUINT64_CLASS, mxREAL);
    *((uint64_T*)(mxGetData(tmp))) = static_cast<double>(sample);
    mxSetFieldByNumber(plhs[0], 0, 3, tmp);
    
    tmp = mxCreateNumericMatrix(1,1, mxUINT32_CLASS, mxREAL);
    *((uint32_T*)(mxGetData(tmp))) = static_cast<uint32_T>(expected);
    mxSetFieldByNumber(plhs[0], 0, 4, tmp);
    
    tmp = mxCreateNumericMatrix(1,1, mxUINT32_CLASS, mxREAL);
    *((uint32_T*)(mxGetData(tmp))) = static_cast<uint32_T>(expected);
    mxSetFieldByNumber(plhs[0], 0, 5, tmp);

    return;
}
                
