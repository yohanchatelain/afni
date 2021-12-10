FROM verificarlo/verificarlo:latest


# Install runtime and basic dependencies
RUN apt-get update && apt-get install -y eatmydata && \
    eatmydata apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    freeglut3-dev \
    git \
    libf2c2-dev \
    libglew-dev \
    libglib2.0-dev \
    libglu1-mesa-dev \
    libglw1-mesa-dev \
    libgsl-dev \
    libgts-dev \
    libjpeg62-dev \
    libmotif-dev \
    libxi-dev \
    libxmhtml-dev \
    libxmu-dev \
    libxpm-dev \
    libxt-dev \
    python3-rpy2 \
    python3-wxgtk4.0 \
    python3.8-dev \
    qhull-bin \
    r-base \
    tcsh \
    xvfb \
    cmake \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*


RUN git clone https://github.com/afni/afni.git && \
    cd afni/ &&\
    CC=verificarlo-c CXX=verificarlo-c++ cmake . &&\
    cd src/ &&\
    make