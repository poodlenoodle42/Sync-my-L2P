FROM ubuntu:18.04

RUN apt-get update && apt-get install -y libssl-dev git build-essential wget file software-properties-common libgl1-mesa-dev
RUN add-apt-repository ppa:beineri/opt-qt-5.15.2-bionic && apt-get update  && apt-get install -y qt515base
RUN mkdir /work
WORKDIR /work
COPY . /work
RUN mkdir /res
ENV PATH="${PATH}:/opt/qt515/bin"
RUN qmake DESTDIR=/res -o syncmyl2p.mk
RUN make -j8 -f syncmyl2p.mk
RUN bash linux/create_appimage.sh