; This file sets emacs variables that are helpful for editing stellard

((nil . ((flycheck-clang-language-standard . "c++11")
         (flycheck-clang-include-path . ("."
                                         "src"
                                         "src/leveldb"
                                         "src/leveldb/port"
                                         "src/leveldb/include"
                                         "src/snappy/snappy"
                                         "src/snappy/config"
                                         ))
         (c-file-style . "stroustrup")
         (compile-command . "scons ccache=1 -D -j $(nproc)"))))
