###################
# CBlas constants #
###################

export Order, Transpose, Uplo, Diag, Side

module Order
    const RowMajor = 101
    const ColMajor = 102
end

module Transpose
    const N = 111   # CblasNoTrans
    const T = 112   # CblasTrans
    const C = 113   # CblasConjTrans
end

module Uplo
    const Upper = 121
    const Lower = 122
end

module Diag
    const NonUnit = 131
    const Unit = 132
end

module Side
    const Left  = 141
    const Right = 142
end
