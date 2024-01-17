pipeline {
    agent { 
        node{
            label "Slater-agent"
        } 
    }
    stages {
        stage('build') {
            steps {
                sh '''#!/bin/bash
                    which gcc
                    which g++
                    cmake -S . -B  build -G Ninja -DCMAKE_CXX_COMPILER=g++ -DCMAKE_C_COMPILER=gcc -Dsimple_parallel_WARNINGS_AS_ERRORS=OFF
                    cmake --build build
                    mkdir release
                    cmake --install build --prefix ./release
                   '''
            }
        }
    }
}