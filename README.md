# Servrian

> My web server written in C just to serve my website.


## Dependencies

- A C compiler; &
- make.


## Compiling 

Simply run:

`make`


## Running

There must be a folder named `web` in the same directory as servrian,
containing at least a `index.html` file. The server will listen on port 8080
if no *PORT* env variable is present. 


# Meta

Distributed under the MIT License. See [LICENSE](LICENSE) for more information.


## Contributing

Check the *contributing* file for details, but, in advance, it is pretty intuitive and straightforward.


## Notes


### MacOS

- Compiling on MacOS with the default C compiler, clang, may give warnings, but compilation finishes without errors;
- You may need to specify the include path to the openssl library because it is, sometimes, in a different place.


### Windows

- Compiling on Windows with Cygwin does not give any warning, but at run time there are several stack traces, I will investigate that.
