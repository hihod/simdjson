#include <cstring>
#ifndef _MSC_VER
#include <dirent.h>
#include <unistd.h>
#else
// Microsoft can't be bothered to provide standard utils.
#include <dirent_portable.h>
#endif
#include <cinttypes>

#include <cstdio>
#include <cstdlib>

#include "simdjson.h"

/**
 * Does the file filename ends with the given extension.
 */
static bool has_extension(const char *filename, const char *extension) {
  const char *ext = strrchr(filename, '.');
  return ((ext != nullptr) && (strcmp(ext, extension) == 0));
}

bool starts_with(const char *pre, const char *str) {
  size_t len_pre = strlen(pre), len_str = strlen(str);
  return len_str < len_pre ? false : strncmp(pre, str, len_pre) == 0;
}

bool contains(const char *pre, const char *str) {
  return (strstr(str, pre) != nullptr);
}

bool validate(const char *dirname) {
  bool everything_fine = true;
  const char *extension = ".json";
  size_t dirlen = strlen(dirname);
  struct dirent **entry_list;
  int c = scandir(dirname, &entry_list, nullptr, alphasort);
  if (c < 0) {
    fprintf(stderr, "error accessing %s \n", dirname);
    return false;
  }
  if (c == 0) {
    printf("nothing in dir %s \n", dirname);
    return false;
  }
  bool *is_file_as_expected = new bool[c];
  for (int i = 0; i < c; i++) {
    is_file_as_expected[i] = true;
  }
  size_t how_many = 0;
  bool needsep = (strlen(dirname) > 1) && (dirname[strlen(dirname) - 1] != '/');
  for (int i = 0; i < c; i++) {
    const char *name = entry_list[i]->d_name;
    if (has_extension(name, extension)) {
      printf("validating: file %s ", name);
      fflush(nullptr);
      size_t filelen = strlen(name);
      char *fullpath = static_cast<char *>(malloc(dirlen + filelen + 1 + 1));
      strcpy(fullpath, dirname);
      if (needsep) {
        fullpath[dirlen] = '/';
        strcpy(fullpath + dirlen + 1, name);
      } else {
        strcpy(fullpath + dirlen, name);
      }
      auto [p, error] = simdjson::padded_string::load(fullpath);
      if (error) {
        std::cerr << "Could not load the file " << fullpath << std::endl;
        return EXIT_FAILURE;
      }
      simdjson::ParsedJson pj;
      ++how_many;
      const int parse_res = json_parse(p, pj);
      printf("%s\n", parse_res == 0 ? "ok" : "invalid");
      if (contains("EXCLUDE", name)) {
        // skipping
        how_many--;
      } else if (starts_with("pass", name) && parse_res != 0) {
        is_file_as_expected[i] = false;
        printf("warning: file %s should pass but it fails. Error is: %s\n",
               name, simdjson::error_message(parse_res).data());
        printf("size of file in bytes: %zu \n", p.size());
        everything_fine = false;
      } else if (starts_with("fail", name) && parse_res == 0) {
        is_file_as_expected[i] = false;
        printf("warning: file %s should fail but it passes.\n", name);
        printf("size of file in bytes: %zu \n", p.size());
        everything_fine = false;
      }
      free(fullpath);
    }
  }
  printf("%zu files checked.\n", how_many);
  if (everything_fine) {
    printf("All ok!\n");
  } else {
    fprintf(stderr,
            "There were problems! Consider reviewing the following files:\n");
    for (int i = 0; i < c; i++) {
      if (!is_file_as_expected[i]) {
        fprintf(stderr, "%s \n", entry_list[i]->d_name);
      }
    }
  }
  for (int i = 0; i < c; ++i) {
    free(entry_list[i]);
  }
  free(entry_list);
  delete[] is_file_as_expected;
  return everything_fine;
}

int main(int argc, char *argv[]) {
#ifndef _MSC_VER
  int c;
  while ((c = getopt(argc, argv, "a:")) != -1) {
    switch (c) {
    case 'a': {
      const simdjson::implementation *impl = simdjson::available_implementations[optarg];
      if (!impl) {
        fprintf(stderr, "Unsupported architecture value -a %s\n", optarg);
        return EXIT_FAILURE;
      }
      simdjson::active_implementation = impl;
      break;
    }
    default:
      fprintf(stderr, "Unexpected argument %c\n", c);
      return EXIT_FAILURE;
    }
  }
#else
  int optind = 1;
#endif
  if ((argc - optind) != 1) {
    std::cerr << "Usage: " << argv[0] << " <directorywithjsonfiles>"
              << std::endl;
#ifndef SIMDJSON_TEST_DATA_DIR
    std::cout
        << "We are going to assume you mean to use the 'jsonchecker' directory."
        << std::endl;
    return validate("jsonchecker/") ? EXIT_SUCCESS : EXIT_FAILURE;
#else
    std::cout << "We are going to assume you mean to use the '"
              << SIMDJSON_TEST_DATA_DIR << "' directory." << std::endl;
    return validate(SIMDJSON_TEST_DATA_DIR) ? EXIT_SUCCESS : EXIT_FAILURE;
#endif
  }
  return validate(argv[optind]) ? EXIT_SUCCESS : EXIT_FAILURE;
}
