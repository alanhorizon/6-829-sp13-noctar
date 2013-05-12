function rx_start_sample = double_window(filename, window_size)
    y = read_cshort_binary(filename);
    % only iterate around the peak
    power = abs(y(7000000:10000000));
    if (nargin < 2)
        window_size = 1000;
    end
    ratio = zeros(length(power)-window_size,1);
    
    for i = 1:length(power)-2*window_size
       w1_sum = sum(power(i:i+window_size));
       w2_sum = sum(power(i+window_size+1:i+2*window_size));
       
       ratio(i) = w2_sum/w1_sum;
       
       if mod(i,10000)==0
           display(i)
       end
       
       
    end
    
    [max_ratio, max_index] = max(ratio)
    
    % compensate for not iterating from start (above)
    rx_start_sample = max_index + 7000000 + window_size + 1;
    
end