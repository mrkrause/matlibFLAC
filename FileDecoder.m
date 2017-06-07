classdef FileDecoder < matlab.mixin.Copyable
    %% FileDecoder Decode data from a FLAC file
    %  
    % This class provides a method to extract data from a FLAC file, in a
    % fast (no re-opening or re-parsing the file) and platform independent
    % way (via libFLAC++). To get at the data, first instantiate a FileDecoder object
    %    decoder = FileDecoder('myfile.flac');
    % Then read segments directly from the file
    %    data{1} = decoder.read_segment(1, 1000); 
    %    data{2} = decoder.read_segment(2000, 3000);
    % The second read should be fast because the file is open, file pointer
    % is in approximately the right place, etc. 
    %
    % You can also read the entire file in one go with decoder.read_file()
    % 
    % The class properties describe various aspects of the encoded file
    % (sample rate, bits per sample, etc). Two specialized getters also
    % provide human-readable descriptions of channel layout and decoder
    % state. 
    %
    % Instances of this class are *not* yet thread-safe.
    % In other words, you should *not* do:
    %   parfor ii=1:n
    %       decoder.read_segment(t(n), t(n+100);
    %   end
    % However, you *can* have multiple decoders pointed at the same file,
    % or clone a buffer via copy(). However, buffer state and position is lost when making a copy
        
    properties (GetAccess = public)
        md5_checking           % If true, verify decoded data against md5 signature
        ogg_serial_number = 1  % Serial number (for ogg only)
        total_samples          % Total number of samples per channel in the file
        channels               % Number of channels in the file
        channel_assignment     % Channel layout--see docs or call get_channel_assignment()
        bits_per_sample        % Bits per sample
        sample_rate            % Sample rate (in samples/sec)
        blocksize              % Number of samples per decoder block.
        decode_position        % Position of the decoder within the file (note that this is bytes, not samples!)
        filename               % File to decode
        state                  % Decoder state (use get_state() for a human-readable code)
    end
    
    properties (SetAccess = private)
       % objectHandle;
        is_initialized = false    % True if initialized
    end
    
    properties (Hidden = true, GetAccess = private)
        objectHandle
    end
    
    
    methods
        function this = FileDecoder(filename, varargin)
            %% Initialize a FileDecoder object
            % INPUT:
            % - filename: filename of FLAC or OGG file. If empty, filename
            %        can also be provided via init()
            % OPTIONAL PARAMETERS:            
            % - md5_checking: Verify decoded data against MD5 signatures
            %        embedded in the file. (Default: false) 
            % - initalize: Initialize the decoder. Once initalized, you
            %        cannot change the other settings. 
            %        (Default: true if filename provided, false otherwise.)
            % - ogg_serial_number: Serial number for OGG decoder
                        
            ip = inputParser();
            ip.addOptional('filename', [], @(x) isempty(x) || ischar(x));
            ip.addParameter('ogg_serial_number', NaN);
            ip.addParameter('md5_checking', false);
            ip.addParameter('initialize', true);
            ip.parse(filename, varargin{:});
            
            this.filename = ip.Results.filename;
           
            this.objectHandle = decoder_interface('new', varargin{:});
            if isfinite(ip.Results.ogg_serial_number)
                this.ogg_serial_number = ip.Results.ogg_serial_number;
            end
            
            this.md5_checking = ip.Results.md5_checking;
            
            if ip.Results.initialize && ~isempty(this.filename)
                this.init(this.filename);
            end
        end
        
        function delete(this)
            %% DELETE Close the decoder and release its internal buffer
            decoder_interface('delete', this.objectHandle);
        end
        
        %% Getters
        function is_checked = get.md5_checking(this)
            is_checked = decoder_interface('get_md5_checking', this.objectHandle);
        end
        
        function serial_number = get.ogg_serial_number(this)
            % Weirdly, there's no library support for this..
            serial_number = this.ogg_serial_number;
        end
        function total_samples = get.total_samples(this)
            total_samples = decoder_interface('get_total_samples', this.objectHandle);            
        end
        
        function n_channels = get.channels(this)
            n_channels = decoder_interface('get_channels', this.objectHandle);
        end
        
        function assignment_code = get.channel_assignment(this)
            assignment_code = decoder_interface('get_channel_assignment', this.objectHandle);
        end
        
        function bps = get.bits_per_sample(this)
            bps = decoder_interface('get_bits_per_sample', this.objectHandle);
        end
        
        function fs = get.sample_rate(this)
            fs = decoder_interface('get_sample_rate', this.objectHandle);
        end
        
        function blocksize = get.blocksize(this)
            blocksize = decoder_interface('get_blocksize', this.objectHandle);
        end
        
        function pos = get.decode_position(this)
            pos = decoder_interface('get_decode_position', this.objectHandle);
        end
        
        function state = get.state(this)
            state = this.get_state();
        end
            
        function [code, msg] = get_channel_assignment(this)
            %% GET_CHANNEL_ASSIGNMENT Return channel assignment code, along with a human-readable string
            
            [code, msg] = decoder_interface('get_channel_assignment', this.objectHandle);  
            if nargout == 0
               disp(msg);
            end
        end
        
        function [code, msg] = get_state(this)
            %% GET_STATE Return state code, along with a human-readable string
            
            [code, msg] = decoder_interface('get_state', this.objectHandle);
             if nargout == 0
               disp(msg);
            end
        end
        
        %% Setters
        function set.md5_checking(this, do_checking) 
            if ~this.is_initialized                                
                decoder_interface('set_md5_checking', this.objectHandle, do_checking);
            else
                error('FileDecoder:AlreadyInitalized', 'Cannot change md5 checking after initialization');
            end
        end
        
        function set.ogg_serial_number(this, serial_number)
             if ~this.is_initialized 
                 decoder_interface('set_ogg_serial_number', this.objectHandle, serial_number);
                 this.ogg_serial_number = serial_number;
             else
                error('FileDecoder:AlreadyInitalized', 'Cannot change serial number after initialization');
            end
        end
        
        
        function init(this, varargin)
            %% INIT Finalize option setting and prepare to decode FLAC data
            if isempty(this.filename) && (nargin < 1 || isempty(varargin{1}))
                error('FileDecoder:NoFilename', 'Filename not set in either constructor or init');
            end
            
            if (nargin > 1) && ~isempty(varargin{1})
                if ~isempty(this.filename) && ~strcmp(this.filename, varargin{1})
                    warning('FileDecoder:ReplacedFilename', 'Overriding filename provided in constructor');
                end
                this.filename = varargin{1};
            end
            
             decoder_interface('init', this.objectHandle, this.filename);
             this.is_initialized = true;
        end
        
        function init_ogg(this, varargin)
            %% INIT_OGG Finalize settings and prepare to decode OGG data
            if isempty(this.filename) && (nargin < 1 || isempty(varargin{1}))
                error('FileDecoder:NoFilename', 'Filename not set in either constructor or init');
            end
            
            if (nargin > 1) && ~isempty(varargin{1})
                if ~isempty(this.filename)
                    warning('FileDecoder:ReplacedFilename', 'Overriding filename provided in constructor');
                end
                this.filename = varargin{1};
            end
            
             decoder_interface('init_ogg', this.objectHandle, this.filename);
             this.is_initialized = true;
        end
        
        function process_single(this)
            %% PROCESS_SINGLE Process a single frame of data or metadata
            % This transfers data to the internal buffer, but does not
            % return it
            if ~this.is_initialized
                this.init();
            end
                        
            ok = decoder_interface('process_single', this.objectHandle);
            if ~ok
                [code, msg] = get_state(this);
                error('FileEncoder:Process', 'Unable to decode. Error # %d. Message %s', code, msg);
            end
        end
        
        function process_until_end_of_metadata(this)
            %% PROCESS_UNTIL_END_OF_METADATA Decode *just* the metadata, stopping at the beginning of the data
            % (Note that, for silly reasons, this does not actually provide
            % *all* of the metadata (like the total number of samples). You
            % need to read one more block for that.
            
            if ~this.is_initialized
                this.init();
            end
            
            ok = decoder_interface('process_until_end_of_metadata', this.objectHandle);
             if ~ok
                [code, msg] = get_state(this);
                error('FileEncoder:Process', 'Unable to decode. Error # %d. Message %s', code, msg);
             end
        end
        
        function process_until_end_of_stream(this)
            %% PROCESS_UNTIL_END_OF_STREAM Decode file, continuing until the end
            % This transfers data into the internal buffer, but does not
            % return it.
            if ~this.is_initialized
                this.init();
            end
            
            ok = decoder_interface('process_until_end_of_stream', this.objectHandle);
            if ~ok
                [code, msg] = get_state(this);
                error('FileEncoder:Process', 'Unable to decode. Error # %d. Message %s', code, msg);
            end
            
        end
        
        function ok = seek_absolute(this, pos)
            %% SEEK_ABSOLUTE Seek to an absolute position within a stream
            % INPUT:
            % - pos: Position in samples (nb: each sample includes a value
            % for all channels), so sample n is indeed the nth value in
            % each channels!
            % OUTPUT:
            % - ok: True if seek is sucessful, false otherwise (see
            % state/get_state() for the reason).
                        
            if ~this.is_initialized
                this.init();
            end
            
            ok = decoder_interface('seek_absolute', this.objectHandle, pos);
        end      
            
        
        function data = read_file(this, varargin)
            %% READ_FILE Read the remaining data in the file and return it
            % INPUT:
            % (none)
            % PARAMETERS
            %  - asDouble: If true, convert to double. Default: true
            % OUTPUT:
            %  - data as an [nSamples x nChannels] matrix
            ip = inputParser();
            ip.addParameter('asDouble', true, @islogical);
            ip.parse(varargin{:});
       
            this.clear_buffer();
            %% Not sure this is necessary but....
            if this.state == 0 
                while this.total_samples == 0
                    this.process_single();
                end
                this.preallocate_buffer(this.total_samples);
            end
            this.process_until_end_of_stream();
            data = this.export_buffer(varargin{:});
                        
        end
        
        function data = read_segment(this, start, stop, varargin)
            %% READ_SEGMENT Read a segment from the file and return it
            % INPUT:
            % - start: first sample to extract
            % - stop:  last sample to extract
            % PARAMETERS:
            % - asDouble: If true, return data as a double. Default: true
            % - seekExact: If true, ensure decoder position is one past the
            %     last returned sample. If false, may be at a block
            %     boundary or something. Default: false
            ip = inputParser();           
            ip.addRequired('start', @(x) x>0);
            ip.addRequired('stop', @(x) x>0);
            ip.addParameter('asDouble', true, @is_logical);
            ip.addParameter('seekExact', false, @is_logical);
            ip.parse(start, stop, varargin{:});
            
            len = stop - start  + 1;
            while this.blocksize == 0
                this.process_single();
            end
            
            blks = ceil(len/this.blocksize);
            
            this.clear_buffer();
            this.preallocate_buffer(len);                        
            
            this.seek_absolute(start-1); % Seeking fills the buffer too....

            for ii=1:blks
                this.process_single();
            end
            data = this.export_buffer(varargin{:});
            data = data(1:this.channels, 1:len);
            
            if ip.Results.seekExact
                this.seek_absolute(stop); % Actually stop+1, since it's zero-indexed
            end
        end
            
        function clear_buffer(this)
            %% CLEAR_BUFFER Clear the internal decoding buffer
            decoder_interface('buffer_clear', this.objectHandle);
        end
        
        function preallocate_buffer(this, sz)
            %% PREALLOCATE_BUFFER Preallocate decoder buffer
            % INPUT:
            % - sz: Number of samples to reserve. Here, 1 sample includes a data point for each channel
            decoder_interface('buffer_preallocate', this.objectHandle, sz*this.channels);
        end
        
        function data = export_buffer(this, varargin)
            %% Export_BUFFER Export decoder buffer to Matlab
            % INPUT:
            %  (none)
            % PARAMETERS:
            % - asDouble: If true, return data as a double. Default: true
            % OUTPUT:
            % - data: [nSamples x nChannels] of doubles or int32
            
            ip = inputParser();
            ip.KeepUnmatched = true;
            ip.addParameter('asDouble', true, @is_logical);
            ip.parse(varargin{:});
            
            if ip.Results.asDouble
                data = double(decoder_interface('buffer_to_matlab', this.objectHandle));
            else
                data = decoder_interface('buffer_to_matlab', this.objectHandle);
            end
        end
    end
    
    methods(Access = protected, Hidden=true)
        function cpObj = copyElement(this)
            if this.is_initialized
                warning('FileDecoder:InitalizedCopy', ...
                    'Decoder has already been initalized--decode position and contents will not be preserved in the copy');
            end
             
            cpObj = FileDecoder(this.filename, ...
                'md5_checking', this.md5_checking, ...
                'ogg_serial_number', this.ogg_serial_number, ...
                'initialize', this.is_initialized);
        end            
    end
    
    methods(Hidden)
        %% Hide some clutter...
        
        function TF = eq(varargin)
            TF = eq@handle(varargin{:});
        end
        
        function TF = ne(varargin)
            TF = ne@handle(varargin{:});
        end
        function TF = ge(varargin)
            TF = ge@handle(varargin{:});
        end
        
        function TF = gt(varargin)
            TF = gt@handle(varargin{:});
        end
        
        function TF = le(varargin)
            TF = le@handle(varargin{:});
        end
        
        function TF = lt(varargin)
            TF = lt@handle(varargin{:});
        end
        
        function Hmatch = findobj(varargin)
            Hmatch = findobj@handle(varargin{:});
        end
        
        function p = findprop(varargin)
            p = findprop@handle(varargin{:});
        end  
    end
end

