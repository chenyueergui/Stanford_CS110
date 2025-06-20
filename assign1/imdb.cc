#include "imdb.h"
#include "imdb-utils.h"
#include <algorithm>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
using namespace std;

const char *const imdb::kActorFileName = "actordata";
const char *const imdb::kMovieFileName = "moviedata";
imdb::imdb(const string &directory) {
  const string actorFileName = directory + "/" + kActorFileName;
  const string movieFileName = directory + "/" + kMovieFileName;
  actorFile = acquireFileMap(actorFileName, actorInfo);
  movieFile = acquireFileMap(movieFileName, movieInfo);
}

bool imdb::good() const {
  return !((actorInfo.fd == -1) || (movieInfo.fd == -1));
}

imdb::~imdb() {
  releaseFileMap(actorInfo);
  releaseFileMap(movieInfo);
}
/**
 * @brief Helper function to construct a film object from raw movie data.
 * @param movieDataPtr Base pointer to the movieFile's memory block.
 * @param offset The offset of the specific movie record.
 * @return A film object.
 */
film buildFilm(const void *movieData, int offset) {
  const char *recordStart = static_cast<const char *>(movieData) + offset;

  // 1. Read movie title
  std::string title(recordStart);

  // 2. Read movie year
  // The year byte is right after the title's null terminator.
  const char *yearPtr = recordStart + title.length() + 1;
  int year = 1900 + static_cast<unsigned char>(*yearPtr);

  return film{title, year};
}

bool imdb::getCredits(const string &player, vector<film> &films) const {
  const char *actorData = static_cast<const char *>(actorFile);
  int numActors = *(int *)actorData;
  const int *offsets_begin = (const int *)actorData + 1;
  const int *offsets_end = offsets_begin + numActors;

  // Use lower_bound to find the potential match
  auto it = lower_bound(offsets_begin, offsets_end, player,
                        [&](int offset, const string &playerName) {
                          return string(actorData + offset) < playerName;
                        });

  // FIX 1: Verify that an exact match was found.
  // Check if iterator is out of bounds OR if the found actor is not the one we
  // are looking for.
  if (it == offsets_end || string(actorData + *it) != player) {
    return false;
  }

  // Now 'it' points to the correct actor's offset.
  const char *recordStart = actorData + *it;

  // --- Parse the actor record ---
  string actorName(recordStart);

  // 1. Calculate size of name field (with padding to be even)
  size_t currentOffset = actorName.length() + 1;
  if (currentOffset % 2 != 0) {
    currentOffset++;
  }

  // 2. Read the number of movies
  const short *moviesCountPtr = (const short *)(recordStart + currentOffset);
  short numMovies = *moviesCountPtr;
  currentOffset += sizeof(short);

  // 3. Handle alignment padding (to be multiple of 4)
  if (currentOffset % 4 != 0) {
    currentOffset += 2;
  }

  // 4. Read the movie offsets
  const int *movieOffsets = (const int *)(recordStart + currentOffset);
  films.clear(); // Ensure the vector is empty before filling
  for (short i = 0; i < numMovies; ++i) {
    films.push_back(buildFilm(movieFile, movieOffsets[i]));
  }

  return true;
}

bool imdb::getCast(const film &movie, vector<string> &players) const {
  const char *movieData = static_cast<const char *>(movieFile);
  int numMovies = *(int *)movieData;
  const int *offsets_begin = (const int *)movieData + 1;
  const int *offsets_end = offsets_begin + numMovies;

  // Use lower_bound to find the potential match
  auto it = lower_bound(offsets_begin, offsets_end, movie,
                        [&](int offset, const film &targetMovie) {
                          // Use the provided helper to reconstruct the film for
                          // comparison
                          return buildFilm(movieFile, offset) < targetMovie;
                        });

  // FIX 1: Verify that an exact match was found.
  film foundFilm = buildFilm(movieFile, *it);
  if (it == offsets_end || !(foundFilm == movie)) { // Use film::operator==
    return false;
  }

  // Now 'it' points to the correct movie's offset.
  const char *recordStart = movieData + *it;

  // --- Parse the movie record ---
  // CLARITY: Reuse foundFilm instead of parsing again.
  // string title = foundFilm.title;

  // 1. Calculate offset past title and year, including padding.
  size_t currentOffset = foundFilm.title.length() + 1; // Past title
  currentOffset += 1;                                  // Past year byte
  if (currentOffset % 2 != 0) {
    currentOffset++; // Padding to make it even
  }

  // 2. Read the number of actors
  const short *castSizePtr = (const short *)(recordStart + currentOffset);
  short castSize = *castSizePtr;
  currentOffset += sizeof(short);

  // 3. Handle alignment padding (to be multiple of 4)
  if (currentOffset % 4 != 0) {
    currentOffset += 2;
  }

  // 4. Read the actor offsets
  const int *actorOffsets = (const int *)(recordStart + currentOffset);
  players.clear(); // Ensure vector is empty
  for (short i = 0; i < castSize; ++i) {
    const char *actorName =
        static_cast<const char *>(actorFile) + actorOffsets[i];
    players.push_back(string(actorName));
  }

  return true;
}

// 声明一个常量指针，指向一个void类型的指针，返回一个imdb::acquireFileMap函数，该函数接受一个string类型的fileName和一个fileInfo类型的info作为参数
const void *imdb::acquireFileMap(const string &fileName,
                                 struct fileInfo &info) {
  // 声明一个stat类型的变量stats
  struct stat stats;
  // 使用stat函数获取fileName的文件信息，并将信息存储在stats中
  stat(fileName.c_str(), &stats);
  // 将stats中的文件大小存储在info的fileSize中
  info.fileSize = stats.st_size;
  // 使用open函数打开fileName文件，以只读方式打开，并将文件描述符存储在info的fd中
  info.fd = open(fileName.c_str(), O_RDONLY);
  // 使用mmap函数将fileName文件映射到内存中，并将映射的地址存储在info的fileMap中
  return info.fileMap =
             mmap(0, info.fileSize, PROT_READ, MAP_SHARED, info.fd, 0);
}

void imdb::releaseFileMap(struct fileInfo &info) {
  if (info.fileMap != NULL)
    munmap((char *)info.fileMap, info.fileSize);
  if (info.fd != -1)
    close(info.fd);
}
