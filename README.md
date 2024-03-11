# Servrian

> A minimal web server written in C.


## Features

- Fast
- Uses the linux sendfile to avoid buffering
- Low memory footprint and very few allocations
- Supports POST, PUT and PATCH by executing files
- Uses only the standard library
- Protection agains ../ path attacks


## Dependencies

- A C compiler; &
- make.


## Compiling 

Simply run:

`make`

To install `make install` will copy the executable to `.local/bin/`.


## Running

There must be a folder named `web` in the same directory as servrian,
containing at least a `index.html` file. The server will listen on port 8080
if no *PORT* env variable is present. 


### Handling requests

By default servrian will route requests using the configured root folder, the
host header and the path; and will add a suffix depending on the accepted
encodings. E.g.: root/host/path. If the same with ending in .gz or .br is
found and the client sent them on the accept-encoding header then that
compressed file is sent.

For POST, PUT and PATCH requests it is also added _.sh_ at end and the file
is executed, passing the method and the content encoding to the callee. The
body is send to standard input, the output generated is directly sent as
response, so programs are responsible for the correct response, including
headers.

Beware that this is a experimental feature and may contain bugs and security
issues. Use with care.


# Meta

Distributed under the MIT License. See [LICENSE](LICENSE) for more information.


## Contributing

Check the *contributing* file for details, but, in advance, it is pretty
intuitive and straightforward.


## Notes


### MacOS

- Compiling on MacOS with the default C compiler, clang, may give warnings, but
  compilation finishes without errors;
- You may need to specify the include path to the openssl library because it is,
  sometimes, in a different place.


### Windows

- Compiling on Windows with Cygwin does not give any warning, but at run time
  there are several stack traces, I will investigate that but fixes are welcome.
