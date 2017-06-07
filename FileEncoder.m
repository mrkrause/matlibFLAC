classdef FileEncoder < handle
    %% FILE ENCODER Write FLAC-compressed data to a file
    %
    % This allows more control over the compression process and is more 
    % portable than using `audiowrite`, which (under the hood) supports a 
    % varying set of options on different versions and platforms.
    % 
    % The interface is very similar to libFLAC++'s FLAC::Encoder::File.
    %
    % First, initialize an Encoder object:
    %   f = FileEncoder('output_file.flac');
    % Next, set various options (if necessary)
    %   f.channels = 3
    %   f.exhaustive_search = true;
    % Finally, submit data to the encoder
    %   f.process(randi(2^15, [n_channels, n_samples], 'int32'));
    %
    % Data may be submitted multiple times, which appends to the file.
    % Obviously, configuration options cannot be set once encoding has begun
    % (i.e., process() or init() has been called). 
    %
    % Once all data has been submitted, finalize the object (or delete it)
    %   f.finish()   
    % 
    % See the libFLAC++ docs at https://xiph.org/flac/api/classFLAC_1_1Encoder_1_1File.html
    % for more details about what each parameter controls.
    properties (SetAccess = protected, Hidden = true)
        objectHandle; % Handle to the underlying C++ class instance
        listener      % This li
    end

    properties (SetObservable)        
        ogg_serial_number;   % Serial number for Ogg
        verify;              % If true, verify by checking that data == decode(encode(data))
        streamable_subset;   % If true, use only the subset of features compatible with streaming
        channels;           % Number of channels to encode
        bits_per_sample;    % Number of bits per sample
        sample_rate;        % Sampling rate (samples/sec)
        compression_level;  % Compression rate (0 [fast,big] to 8 [slow,small])
        blocksize;          % Samples per frame. If zero, encoder estimates it itself
        mid_side_stereo;      % If true, use mid-size encoding on stereo input. If false, code channels indepedently. Ignored unless channels == 2
        loose_mid_side_stereo; %If true, adaptively switch between mid-size and left-right encoding. If false, do exhaustive search. Ignored unless mid_size_stereo is set
        apodization;        % Specify apodiziation functions
        max_lpc_order;      % Maximum LPC order. If zero, uses only fixed predictors
        qlp_coeff_precision; % Precision, in bits, of the quantized linear predictor coefficients. If zero, encoder selects it. Must be <32
        qlp_coeff_prec_search; % If true, search neighboring qlp values to find the best one. Otherwise, use only specified precision.
        exhaustive_model_search; % If true, encoder searches all order models. If false, estimates the best based on residual signal energy. 
        min_residual_partition_order; % Minimum partition order used for coding the residual. 
        max_residual_partition_order; % Maximum partition order used for coding the residual. 
        total_samples_estimate; % Estimated number of samples (used to avoid rewriting STREAMTABLE at end of encoding).        
    end
    
    properties (SetAccess=protected)
        filename; %Output filename
        is_initialized = false;
    end

    
    methods (Static)
        function PreSetHandler(src, ev)
            % This callback prevents setting configuration options after
            % encoding has already begun.
            
            is_initalized = ev.AffectedObject(1).is_initialized;
            if is_initalized
                error('FileEncoder:AlreadyInitalized', 'Cannot set %s because the encoder has already been initalized', src.N);
            end
        end
    end
    
    events
        PreSet
    end
    
    methods
        function this = FileEncoder(varargin)
            %% Construct a FileEncoder object
            ip = inputParser();
            ip.addOptional('filename', [], @(x) isempty(x) || ischar(x));
            ip.parse(varargin{:});
                                                            
            this.objectHandle = encoder_interface('new', varargin{:});
            this.filename = ip.Results.filename;
            this.compression_level = 5;
            
            fixed_properties = {...
                'ogg_serial_number', 'verify', 'streamable_subset', ...
                'channels', 'bits_per_sample', 'sample_rate', ...
                'compression_level', 'blocksize', 'mid_side_stereo', ...
                'loose_mid_side_stereo', 'apodization', 'max_lpc_order', ...
                'qlp_coeff_precision', 'qlp_coeff_prec_search', ....
                'exhaustive_model_search', 'min_residual_partition_order', ...
                'max_residual_partition_order', 'total_samples_estimate'};
            for f=1:length(fixed_properties)
                this.listener{end+1} = addlistener(this, fixed_properties{f}, 'PreSet', @FileEncoder.PreSetHandler);
            end
        end
        
        function delete(this)
            encoder_interface('delete', this.objectHandle);
        end

        function serial_no = get.ogg_serial_number(this)
            % Inexplicably, this is not actually implemented in the library
            serial_no = this.ogg_serial_number;
        end
                
        function is_verify = get.verify(this)
            is_verify = encoder_interface('get_verify', this.objectHandle);
        end
        
        function is_streamable = get.streamable_subset(this)
            is_streamable = encoder_interface('get_streamable_subset', this.objectHandle);
        end
        
        function n_channels = get.channels(this) 
            n_channels = encoder_interface('get_channels', this.objectHandle);
        end
        
        function bit_depth = get.bits_per_sample(this)
            bit_depth = encoder_interface('get_bits_per_sample', this.objectHandle);
        end
        
        function fs = get.sample_rate(this)
            fs = encoder_interface('get_sample_rate', this.objectHandle);
        end
        
        function level =  get.compression_level(this)
            level = this.compression_level;
        end
                
        function block_sz = get.blocksize(this)
            block_sz = encoder_interface('get_blocksize', this.objectHandle);
        end
        
        function is_mid_side = get.mid_side_stereo(this)
            is_mid_side = encoder_interface('get_do_mid_side_stereo', this.objectHandle);
        end
        
        function is_loose = get.loose_mid_side_stereo(this)
             is_loose = encoder_interface('get_loose_mid_side_stereo', this.objectHandle);
        end
        
        function windows = get.apodization(this)
            windows = this.apodization; %No support for this in libFLAC, so we track it locally
        end
        
        function order = get.max_lpc_order(this)
            order = encoder_interface('get_max_lpc_order', this.objectHandle);
        end
        
        function precision = get.qlp_coeff_precision(this)
            precision = encoder_interface('get_qlp_coeff_precision', this.objectHandle);
        end
        
        function is_searching = get.qlp_coeff_prec_search(this)
            is_searching = encoder_interface('get_qlp_coeff_prec_search', this.objectHandle);
        end
        
        function is_searching = get.exhaustive_model_search(this)
            is_searching = encoder_interface('get_do_exhaustive_model_search', this.objectHandle);
        end
        
        function order = get.min_residual_partition_order(this)
            order =  encoder_interface('get_min_residual_partition_order', this.objectHandle);
        end
        
        function order = get.max_residual_partition_order(this)
            order =  encoder_interface('get_max_residual_partition_order', this.objectHandle);
        end
        
        function order = get.total_samples_estimate(this)
            order =  encoder_interface('get_total_samples_estimate', this.objectHandle);
        end
        
        function [code, msg] = get_state(this)
            [code, msg] = encoder_interface('get_state', this.objectHandle);
        end
        
        %%%%% Setters %%%%%%
        function set.ogg_serial_number(this, sn)            
            encoder_interface('set_ogg_serial_number', this.objectHandle, sn);
            this.ogg_serial_number = sn;
        end
        
        
        function set.verify(this, new_value)
            if ~is_logicalish(new_value) && numel(new_value) == 1
                error('FileEncoder:SetVerify', 'Cannot set verify to new value (must be logical scalar)');
            end
            encoder_interface('set_verify', this.objectHandle, logical(new_value));
            this.verify = encoder_interface('get_verify', this.objectHandle);
        end
        
        
        function set.streamable_subset(this, new_value)
             if ~is_logicalish(new_value)
                error('FileEncoder:SetVerify', 'Cannot set streamable_subset to new value (must be logical)');
             end
             
             encoder_interface('set_streamable_subset', this.objectHandle, logical(new_value));            
        end
        
        
        function set.channels(this, nchannels)
            if ~is_int(nchannels) && numel(nchannels) ~= 1
                error('FileEncoder:SetChannels', 'Number of channels must be a scalar integer (regardless of actual type), not %d', nchannels);
            end
            
            encoder_interface('set_channels', this.objectHandle, nchannels);
        end
        
        
        function set.bits_per_sample(this, bps)
            if bps <= 0 || ~is_int(bps)
                error('FileEncoder:BitsPerSample', 'Bits per sample must be a positive integer');
            end
            
            encoder_interface('set_bits_per_sample', this.objectHandle, bps);
        end
        
        
        function set.sample_rate(this, fs)
            if ~is_int(fs) || fs <= 0
                error('FileEncoder:SetSampleRate', 'Sample Rate must be a positive integer (regardless of actual type), not %d', nchannels);
            end
            
            encoder_interface('set_sample_rate', this.objectHandle, fs);
        end

        
        function set.compression_level(this, level)            
            if ~is_int(level) && numel(level) == 1 && level > 0
                error('FileEncoder:SetCompression', 'Compression must be a scalar integer in [0,8], not %d', nchannels);
            end
            
            if level > 8
                warning('FileEncoder:SetCompression', 'Compression level above 8 replaced with 8');
                level = 8;
            end
            
            encoder_interface('set_compression_level', this.objectHandle, level);
            this.compression_level = level; % No library support for this(?!)
            
            this.apodization = {'tukey(0.5)'};
            if level > 5
                 this.apodization{end+1} = 'partial_tukey(2)';
            end
            if level > 7
                this.apodization{end+1} = 'punchout_tukey(3)';
            end            
        end
        
        
        function set.blocksize(this, blksz)
            if ~is_int(blksz) || blksz <= 0
                error('FileEncoder:SetBlocksize', 'Blocksize must be a positive integer (regardless of actual type), not %d', nchannels);
            end
            
            encoder_interface('set_blocksize', this.objectHandle, blksz);
        end
            
        
        function set.mid_side_stereo(this, is_mid)
            if ~(is_logicalish(is_mid) &&  isscalar(is_mid))
                error('FileEncoder:SetMidSideStereo', 'Mid-Side Stereo must be a logical scalar');                
            end
                       
            encoder_interface('set_mid_side_stereo', this.objectHandle, is_mid);
        end
        
        
        function set.loose_mid_side_stereo(this, is_loose)
            if ~(is_logicalish(is_loose) && isscalar(is_loose))
                error('FileEncoder:SetLooseMidSideStereo', 'Loose Mid-Side Stereo must be a logical scalar');                
            end
                       
            encoder_interface('set_loose_mid_side_stereo', this.objectHandle, is_loose);
        end
        
        
        function set.apodization(this, windows)
            if ischar(windows)
                windows = strsplit(windows, ';'); %
            elseif ~iscell(windows)
                error('FileEncoder:WindowFormat', 'FileEncoder expects a cell array of window specs (or semicolon-separated char array');
            end
            
            if length(windows) > 32
                warning('FileEncoder:TooManyWindows', 'Received %d apodization windows, but the maximum is 32. The last %d windows will be ignored', length(windows), length(windows)-32);
            elseif isempty(windows)
                error('FileEncoder:NotEnoughWindows', 'At least one apodization window must be provided! tukey(0.5) is a reasonable default.')
            end

            windows = windows(1:min(length(windows), 32));
            winstr = '';
            for ii=1:length(windows)
                [ok, msg] = FileEncoder.check_window(windows{ii});
                if ok
                    if isempty(winstr)
                        winstr = windows{ii};
                    else
                        winstr = [winstr ';', windows{ii}]; %#ok<AGROW>
                    end
                else
                    error('FileEncoder:BadWindow', msg);
                end
            end
            
            this.apodization = windows;
            fprintf('Setting apodization to %s', winstr);
            encoder_interface('set_apodization', this.objectHandle, winstr);
        end
        
        
        function set.max_lpc_order(this, order)
            if ~(is_int(order) && isscalar(order) && order >= 0)
                error('FileEncoder:SetMaxLPCOrder', 'The maximum LPC order must be a non-negative scalar');  
            end
            
             encoder_interface('set_max_lpc_order', this.objectHandle, order);
        end

        
        function set.qlp_coeff_precision(this, precision)
            if ~(is_int(precision) && isscalar(precision) && precision >= 0)
                error('FileEncoder:SetQLPCoeffPrecision', 'QLP precision must be a non-negative scalar');  
            end
             
            encoder_interface('set_qlp_coeff_precision', this.objectHandle, precision);            
        end
        
        
        function set.qlp_coeff_prec_search(this, do_search)
            if ~(is_logicalish(do_search) && isscalar(do_search))
                error('FileEncoder:SetQLPCoeffPrecSearch', 'QLP Precision Searching must be a logical scalar');
            end
            
            encoder_interface('set_do_qlp_coeff_prec_search', this.objectHandle, do_search);
        end
        
        
        function set.exhaustive_model_search(this, do_search)
             if ~(is_logicalish(do_search) && isscalar(do_search))
                 error('FileEncoder:SetExhaustiveModelSearch', 'Exhaustive model search must be a logical scalar');
             end
             
              encoder_interface('set_do_exhaustive_model_search', this.objectHandle, do_search);
        end
        
        
        function set.min_residual_partition_order(this, order)
            if ~(is_int(order) && isscalar(order) && order >= 0) 
                 error('FileEncoder:SetMinResidualPartitionOrder', 'Minimum residual partition order must be a non-negative scalar');
            end
            
             encoder_interface('set_min_residual_partition_order', this.objectHandle, order);             
        end
        
        
        function set.max_residual_partition_order(this, order)
            if ~(is_int(order) && isscalar(order) && order >=0)
                 error('FileEncoder:SetMaxResidualPartitionOrder', 'Maximum residual partition order must be a non-negative scalar');
            end
            
            encoder_interface('set_max_residual_partition_order', this.objectHandle, order);             
        end
        
         
        function set.total_samples_estimate(this, estimate)
            if ~(is_int(estimate) && isscalar(estimate) && estimate >=0)
                 error('FileEncoder:SetMaxResidualPartitionOrder', 'Total samples estimate must be a non-negative scalar');
            end
            
            encoder_interface('set_total_samples_estimate', this.objectHandle, estimate);             
        end
        
        
        function ok = finish(this)
             ok = encoder_interface('finish', this.objectHandle);
             if ~ok
                 [code, msg] = this.get_state();
                 error('FileEncoder:FinishError', 'Error finishing FLAC file (code %d): %s', code, msg);
             end    
        end
        
        
        function init(this, varargin)
             %% INIT Finalize option setting and prepare to encode data
            if isempty(this.filename) && (nargin > 1 && isempty(varargin{1}))
                error('FileEncoder:NoFilename', 'Filename not set in either constructor or init');
            end
            
            if (nargin > 1) && ~isempty(varargin{1})
                if ~isempty(this.filename)
                    warning('FileEncoder:ReplacedFilename', 'Overriding filename provided in constructor');
                end
                this.filename = varargin{1};
            end
            
             encoder_interface('init', this.objectHandle, this.filename);
             this.is_initialized = true;
        end
                 
        
        function process(this, data)
             %% PROCESS: Submit a batch of data for encoding.
              % Data should arranged in an n_channels x n_samples matrix and compatible with the 
              % selected bit-depth. For example, if using 16 bits/sample, values must be
              % between (-2^15) and (2^15 - 1). Data is cast to an int32
              % before sending to libFLAC, which is the largest/only size
              % libFLAC supports.
              %
              % Implementation note: This calls process_interleaved
              % internally. 
             if ~this.is_initialized
                 this.init();
             end
             
             n_ch = this.channels;
             if(n_ch > 1 && (size(data, 1) ~= n_ch) || ~ismatrix(data))
                 error('FileEncoder:InputDataShape', ...
                     'Data submitted for processing in the wrong shape: Should be %d (channels) x nsamples)',...
                     n_ch);
             end
             
             ok = encoder_interface('process_interleaved', this.objectHandle, int32(data));
             if ~ok
                 [code, msg] = this.get_state();
                 error('FileEncoder:Process', ...
                     'Unable to process submitted data. Error code %d (%s)', code, msg);
             end
         end                   
    end
    
    
    methods(Static)
        function [windows, parameterized_windows] = get_apodization_windows()
            %% GET_APODIZATION_WINDOWS Return a list of possible apodization windows
            % [windows, parameterized_windows] = get_apodization_windows()
            % The windows in windows take no parameters, while those in 
            % parameterized_windows take either:
            % - n: The number of windows (int)
            % - n/ov: Num. of windows and an overlap (<1, may be <0)
            % - n/ov/P: Num. of windows, overlap, taper proportion (0<x<1)
            
            persistent wins
            persistent pwins
            
            if isempty(wins)
                wins = { ...
                    'bartlett', ...
                    'bartlett_hann',...
                    'blackman', ...
                    'blackman_harris_4term_92db', ...
                    'connes', ...
                    'flattop', ...                    
                    'hamming', ...
                    'hann', ...
                    'kaiser_bessel',...
                    'nuttall', ...
                    'rectangle', ...
                    'triangle', ...
                    'tukey(P)',...                                        
                    'welch'
                    };
                
                pwins = {...
                    'gauss(STDDEV)', ...
                    'partial_tukey(n[/ov[/P]])', ...
                    'punchout_tukey(n[/ov[/P]])',...
                    };
            end
            windows = wins;
            parameterized_windows = pwins;
        end
    
        
        function [ok, msg] = check_window(win)
            % CHECK_WINDOW Check an apodization window for validity.
            % Checks parameters too, where necessary.
            % Note that the libFLAC encoder can silently discard invalid
            % apodization windows, so we check here instead.
            windows = FileEncoder.get_apodization_windows();
    
            ok = false;
            msg = '';
            if ismember(win, windows)
                ok = true;
                return;
            end
    
            [is_match, tokens] = regexp(win, 'gauss\((\d*.?\d+)\)', 'start', 'tokens');
            if is_match == 1
                tokens = str2double(tokens{1}{1});

                if (tokens > 0 && tokens <= 0.5)
                    ok = true;
                    return;
                else
                    ok = false;
                    msg = sprintf('Standard Deviation for a Gaussian window must be in 0 < x <= 0.5, not %f', tokens);
                end
            end
            
            [is_match, tokens] = regexp(win, 'tukey\((-?0?.?\d+)\)', 'start', 'tokens');
            if is_match == 1
                tokens = str2double(tokens{1}{1});

                if (tokens >= 0 && tokens <= 1)
                    ok = true;
                    return;
                else
                    ok = false;
                    msg = sprintf('Standard Deviation for a Tukey window must be in 0 <= x <= 1, not %d', tokens);
                end
            end
            
            if win(end) == ')' && ...
                strncmp(win, 'partial_tukey(', length('partial_tukey(')) || ...
                strncmp(win, 'punchout_tukey(', length('punchout_tukey('))
                %%try
                    tokens = regexp(win, '\((.+)\)', 'tokens');
                    tokens = strsplit(tokens{1}{1}, '/');
                    tokens = cellfun(@str2double, tokens);
                
                    t_ok = false(3,1);
                    
                    if length(tokens) == 3
                        if tokens(3) >= 0 && tokens(3) <= 1
                            t_ok(3) = true;
                        else
                            msg = sprintf('P parameter must be between 0 and 1, not %d', tokens(3));
                        end
                    else
                        t_ok(3) = true;
                    end
                                        
                    if length(tokens) >= 2
                        if tokens(2) <= 1
                            t_ok(2) = true;
                        else
                            msg = sprintf('Overlap must be less than one (but may be negative), not %d', tokens(2));
                        end
                    else
                        t_ok(2) = true;
                    end
                    
                    if length(tokens) >=1
                        if tokens(1) > 0 && is_int(tokens(1))
                            t_ok(1) = true;
                        else
                            msg = sprintf('Number must be a positive integer, not %d', tokens(1));
                        end
                    end
                    
                    ok = all(t_ok);
                try
                catch
                    ok = false;
                    msg = sprintf('Could not parse window description %s', win);
                end
            end
             
            if ~ok && isempty(msg)
                msg = sprintf('Unknown window %s', win);
            end
        end 
    end
end

function int_p = is_int(x)
%% IS_INT: Returns true iff x is an integer (even if stored as a double, etc).
int_p = ~islogical(x) & (mod(x,1) == 0);
end

function bool_p = is_logicalish(x)
%% IS_LOGICALISH: Returns true iff x is a logical-ish variable (true, false, 0, 1)
bool_p = islogical(x) || (x == 0 || x == 1);
end

    