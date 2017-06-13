%% Edit the line below to point to your copy of libFLAC
FLAC_PATH = ''';

if isempty(FLAC_PATH)
  warningr('matlibFLAC::Build', 'You probably need to provide a path to libFLAC');
end

files= {
    'encoder_interface.cpp', ...
    'decoder_interface.cpp'
    };

for f=1:length(files)
    try
    mex('-v', ...
        sprintf('-I%s', fullfile(FLAC_PATH, 'include', '')), ...
        sprintf('-L%s', fullfile(FLAC_PATH, 'lib', '')), ...
        '-lFLAC', ...
        '-lFLAC++', ...
        files{f});
    catch E
        disp(E.message);
    end
end
   
