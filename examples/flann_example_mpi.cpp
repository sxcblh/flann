
#include <flann/flann_mpi.hpp>
#include <flann/flann.hpp>
#include <flann/io/hdf5.h>

#include <stdio.h>

#define IF_RANK0 if (world.rank()==0)

clock_t start_time_;

void start_timer(const std::string& message = "")
{
	if (!message.empty()) {
		printf("%s", message.c_str());
		fflush(stdout);
	}
	start_time_ = clock();
}

double stop_timer()
{
	return double(clock()-start_time_)/CLOCKS_PER_SEC;
}

float compute_precision(const flann::Matrix<int>& match, const flann::Matrix<int>& indices)
{
	int count = 0;

	assert(match.rows == indices.rows);
	size_t nn = std::min(match.cols, indices.cols);

	for(size_t i=0; i<match.rows; ++i) {
		for (size_t j=0;j<nn;++j) {
			for (size_t k=0;k<nn;++k) {
				if (match[i][j]==indices[i][k]) {
					count ++;
				}
			}
		}
	}

	return float(count)/(nn*match.rows);
}


int main(int argc, char** argv)
{
	boost::mpi::environment env(argc, argv);
	boost::mpi::communicator world;

	int nn = 1;

	//flann::Matrix<float> dataset;
	flann::Matrix<float> query;
	flann::Matrix<int> match;
	flann::Matrix<float> gt_dists;

	IF_RANK0 start_timer("Loading data...\n");
	//flann::load_from_file(dataset, "sift100K.h5","dataset");
	flann::load_from_file(query, "sift100K.h5","query");
	flann::load_from_file(match, "sift100K.h5","match");
	flann::load_from_file(gt_dists, "sift100K.h5","dists");
	flann::Matrix<int> indices(new int[query.rows*nn], query.rows, nn);
	flann::Matrix<float> dists(new float[query.rows*nn], query.rows, nn);

	// construct an randomized kd-tree index using 4 kd-trees
	flann::mpi::Index<flann::L2<float> > index("sift100K.h5", "dataset", flann::KDTreeIndexParams(4));
	//flann::Index<flann::L2<float>	> index( dataset, flann::KDTreeIndexParams(4));
	IF_RANK0 printf("Loading data done (%g seconds)\n", stop_timer());

	IF_RANK0 printf("Index size: (%d,%d)\n", index.size(), index.veclen());

	start_timer("Building index...\n");
	index.buildIndex();                                                                                               
	printf("Building index done (%g seconds)\n", stop_timer());
    
    world.barrier();

	// do a knn search, using 128 checks
	IF_RANK0 start_timer("Performing search...\n");
	index.knnSearch(query, indices, dists, nn, flann::SearchParams(128));
	IF_RANK0 printf("Search done (%g seconds)\n", stop_timer());
	IF_RANK0 {
		printf("Indices size: (%d,%d)\n", (int)indices.rows, (int)indices.cols);
		printf("Checking results\n");
		float precision = compute_precision(match, indices);
		printf("Precision is: %g\n", precision);
	}
	delete[] query.data;
	delete[] indices.data;
	delete[] dists.data;
	delete[] match.data;

	return 0;
}
