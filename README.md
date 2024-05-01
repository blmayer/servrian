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

- Crypt;
- Shadow;
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
body is sent to standard input, the output generated is directly sent as
response, so programs are responsible for the correct response, including
headers.

Beware that this is a experimental feature and may contain bugs and security
issues. Use with care.


### Authorization

Servrian leverages unix tools to handle authorization and authentication, for
most cases it should work out of the box. Whenever a file requested is not
publicly readable, this is, the file doesn't have the `a+r` flag, then servrian
checks for the *Authorization* HTTP header, if present it is used to authorize
using your system's */etc/passwd* file, the same way you login into your shell.

If the header is not present status 401 is returned with a `WWW-Authenticate`
header, only the *Basic* method is supported now. For shadow passwords your
user must have read access to the shadow file.


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
