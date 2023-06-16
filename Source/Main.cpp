#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <Windows.h>
#include <filesystem>
#include <stb_image_write.h>

#define TAG_IMG_METADATA 1
#define TAG_IMG_PIXELS 2
#define TAG_EXIT 100

#define USE_ASYNC_RECV
#define USE_TESTSOME

// MPI global state.
struct {
    int commSize;
    int commRank;
} mpi;

void processImage(uint8_t* pixels, int width, int height, int comp);

struct image_t {
    int width;
    int height;
    int components;
    uint8_t* pixels;
    char name[256];

    int size() const { return width * height * 3; }
};

struct task_t {
    image_t img;
    int node;
    MPI_Request request;
    MPI_Request response;
};

template <typename... arg_ts>
void print(const char* fmt, arg_ts&&... args)
{
    printf(fmt, args...);
    printf("\n");
    fflush(stdout);
}

void main_master(int argc, char** argv)
{
    auto start = std::chrono::steady_clock::now();
#ifdef USE_ASYNC_RECV
    int numTasks = 0;
    task_t* tasks = (task_t*)malloc(mpi.commSize * sizeof(task_t));
    memset(tasks, 0, mpi.commSize * sizeof(task_t));

    MPI_Request* responses = (MPI_Request*)malloc(mpi.commSize * sizeof(MPI_Request));
    for (int i = 0; i < mpi.commSize; ++i)
        responses[i] = MPI_REQUEST_NULL;
#ifdef USE_TESTSOME
    int numCompleteResponses = 0;
    int* completeResponses = (int*)malloc(mpi.commSize * sizeof(int));
#endif

    int numFreeNodes = mpi.commSize - 1;
    int* freeNodes = (int*)malloc(mpi.commSize * sizeof(int));
    for (int i = 1; i < mpi.commSize; ++i)
        *freeNodes++ = i;
#endif

    char message[128];
    std::filesystem::create_directories("output");

    int exit = 0;
    int dest = 1;
#if 0 // breakpoint
    while (!IsDebuggerPresent())
            ;
#endif

#ifdef USE_ASYNC_RECV
    auto process_responses = [&]() {
        int index, complete;
#ifdef USE_TESTSOME
        MPI_Testsome(mpi.commSize, responses, &numCompleteResponses, completeResponses, MPI_STATUS_IGNORE);
        for (int i = 0; i < numCompleteResponses; ++i) {
            index = completeResponses[i];
#else
        MPI_Testany(mpi.commSize, responses, &index, &complete, MPI_STATUS_IGNORE);
        if (complete) {
#endif
            auto& task = tasks[index];

            // Save image.
            auto path_fs = std::filesystem::current_path() / "output" / task.img.name;
            stbi_write_jpg(
                path_fs.string().c_str(),
                task.img.width,
                task.img.height,
                task.img.components,
                task.img.pixels,
                100);

            //print("[%3d] DONE    %s", task.node, task.img.name);

            // Return node to the free list.
            *freeNodes++ = task.node;
            numFreeNodes++;
            numTasks--;

            // Invalidate response request.
            responses[task.node] = MPI_REQUEST_NULL;

            free(task.img.pixels);
            memset(&task, 0, sizeof(task_t));
        }
    };
#endif

    auto iter = std::filesystem::directory_iterator(argv[0]);
    auto dir_entry = std::filesystem::begin(iter);
    while (dir_entry != std::filesystem::end(iter)) {
        if (!dir_entry->is_regular_file()) {
            dir_entry++;
            continue;
        }

#ifdef USE_ASYNC_RECV
        // Check if any node has processed an image.
        if (numTasks > 0) {
            process_responses();
        }

        // Check if we can process the next file.
        if (numFreeNodes > 0) {
#endif
            std::string file = dir_entry->path().string();
#ifdef USE_ASYNC_RECV
            dest = *--freeNodes;
            numFreeNodes--;

            task_t& task = tasks[dest];
#else
        dest = (dest % (mpi.commSize - 1)) + 1;
        task_t task = {};
#endif
            task.node = dest;
            task.img.pixels = stbi_load(file.c_str(), &task.img.width, &task.img.height, NULL, 3);
            task.img.components = 3;
            if (task.img.pixels == NULL) {
                print("Error in loading the image %s", file.c_str());
                continue;
            }
            strcpy(task.img.name, dir_entry->path().filename().string().c_str());
#ifdef USE_ASYNC_RECV
            numTasks++;
#endif

            // Tell the not-a-master that we have data to process.
            MPI_Send(&exit, 1, MPI_INT, task.node, TAG_EXIT, MPI_COMM_WORLD);

            // Send the image.
            MPI_Send(&task.img, 3, MPI_INT, task.node, TAG_IMG_METADATA, MPI_COMM_WORLD);
            MPI_Send(task.img.pixels, task.img.size(), MPI_UINT8_T, task.node, TAG_IMG_PIXELS, MPI_COMM_WORLD);
            //print("[%3d] SENT    %s", task.node, task.img.name);

#ifdef USE_ASYNC_RECV
            // Receive processed image.
            MPI_Irecv(task.img.pixels, task.img.size(), MPI_UINT8_T, task.node, TAG_IMG_PIXELS, MPI_COMM_WORLD, &responses[task.node]);
#else
        MPI_Recv(task.img.pixels, task.img.size(), MPI_UINT8_T, task.node, TAG_IMG_PIXELS, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // Save image.
        auto path_fs = std::filesystem::current_path() / "output" / task.img.name;
        stbi_write_jpg(
            path_fs.string().c_str(),
            task.img.width,
            task.img.height,
            task.img.components,
            task.img.pixels,
            100);

        print("[%3d] DONE    %s", task.node, task.img.name);
        free(task.img.pixels);
        memset(&task, 0, sizeof(task_t));
#endif

            dir_entry++;
        }
#ifdef USE_ASYNC_RECV
    }
    while (numTasks > 0) {
        process_responses();
    }
#endif

    // Execute order 66 >:(
    exit = 1;
    for (int i = 1; i < mpi.commSize; ++i) {
        MPI_Send(&exit, 1, MPI_INT, i, TAG_EXIT, MPI_COMM_WORLD);
    }
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    print("seconds:%f ", elapsed_seconds.count());
}

void main_not_a_slave_but_not_a_master()
{
    cuInit(0);

    while (true) {
        // Continue or exit.
        int exitRequested = 0;
        MPI_Recv(&exitRequested, 1, MPI_INT, 0, TAG_EXIT, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if (exitRequested)
            break;

        // Receive the image.
        image_t im = {};
        MPI_Recv(&im, 3, MPI_INT, 0, TAG_IMG_METADATA, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        im.pixels = (uint8_t*)malloc(im.size() * sizeof(uint8_t));
        MPI_Recv(im.pixels, im.size(), MPI_UINT8_T, 0, TAG_IMG_PIXELS, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // Launch CUDA kernel.
        processImage(im.pixels, im.width, im.height, im.components);

        // Send the processed image.
        MPI_Send(im.pixels, im.size(), MPI_UINT8_T, 0, TAG_IMG_PIXELS, MPI_COMM_WORLD);
        free(im.pixels);
    }
}

int main(int argc, char** argv)
{
    argc--;
    argv++;
    MPI_Init(&argc, &argv);

    // Get number of processes and index of the current process (rank).
    MPI_Comm_size(MPI_COMM_WORLD, &mpi.commSize);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi.commRank);

    MPI_Status status = {};

    if (argc > 0) {
        if (mpi.commRank == 0)
            main_master(argc, argv);
        else
            main_not_a_slave_but_not_a_master();
    }

    MPI_Finalize();
}
