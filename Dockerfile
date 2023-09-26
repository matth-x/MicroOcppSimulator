# Use Ubuntu as the base image
FROM ubuntu:latest

# Update package lists and install necessary dependencies
RUN apt-get update && apt-get install -y \
    git \
    cmake \
    libssl-dev \
    build-essential \
    && rm -rf /var/lib/apt/lists/*

# Set the working directory inside the container
WORKDIR /MicroOcppSimulator

# Copy your application files to the container's working directory
COPY . .

RUN git clone --recurse-submodules https://github.com/matth-x/MicroOcppSimulator
RUN cd MicroOcppSimulator && mkdir build && mkdir mo_store
RUN cmake -S . -B ./build
RUN cmake --build ./build -j 16 --target mo_simulator

# Grant execute permissions to the shell script
RUN chmod +x /MicroOcppSimulator/build/mo_simulator

# Expose port 8000
EXPOSE 8000

# Run the shell script inside the container
CMD ["./build/mo_simulator"]