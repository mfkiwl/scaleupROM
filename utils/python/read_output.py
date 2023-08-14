import h5py
import numpy as np
import argparse

parser = argparse.ArgumentParser(description = "",
                                 formatter_class=argparse.RawTextHelpFormatter)
parser.add_argument('dir_format', metavar='string', type=str,
                    help='directory format for result files.\n')
parser.add_argument('file_format', metavar='string', type=str,
                    help='filename format for result files.\n')
parser.add_argument('nsample', metavar='integer', type=int,
                    help='number of samples\n')
parser.add_argument('--dir-indexes', nargs='+', type=int)

def collectResults(format_, nSample, attrs):
    filenames = [format_ % k for k in range(nSample)]
    
    results = np.zeros((len(attrs), nSample), dtype=float)
    
    for k, filename in enumerate(filenames):
        with h5py.File(filename, 'r') as f:
            for a, attr in enumerate(attrs):
                results[a, k] = f.attrs[attr]
    
    results = np.sort(results)

    stats = []

    med = int(nSample * 0.5)
    stdp = int(nSample * (0.5 + 0.34))
    stdm = int(nSample * (0.5 - 0.34))
    for a, attr in enumerate(attrs):
        stats += [[results[a, med],
                   results[a, 0],
                   results[a, -1],
                   results[a, stdm],
                   results[a, stdp]]]
        print("================================")
        print(attr)
        print("median: %.5E" % stats[-1][0])
        print("minimum: %.5E" % stats[-1][1])
        print("maxinum: %.5E" % stats[-1][2])
        print("-1std: %.5E" % stats[-1][3])
        print("+1std: %.5E" % stats[-1][4])
        print("================================")
    
    return stats

if __name__ == "__main__":
    attrs = ["rom_assemble", "rom_solve", "fom_assemble", "fom_solve", "rel_error"]
    #dir_format = "scaleup-%dx%d-output"
    #file_format = "result_%d.h5"
    args = parser.parse_args()

    attr_stats = [[] for k in range(len(attrs))]
    #dir_list = [4,8,16,32,64,128]
    for size in args.dir_indexes:
        dirname = args.dir_format % size
        print("directory: %s" % dirname)

        format_ = "%s/%s" % (dirname, args.file_format)
        stats = collectResults(format_, args.nsample, attrs)
        for a, attr in enumerate(attrs):
            attr_stats[a] += [[size] + stats[a]]

    for a, attr in enumerate(attrs):
        np.savetxt('%s.txt' % attr, np.array(attr_stats[a]), fmt='%.5E')
