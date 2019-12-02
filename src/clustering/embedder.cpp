#include "embedder.h"
#include "../document.h"

#include <sstream>
#include <cassert>

FastTextEmbedder::FastTextEmbedder(
    fasttext::FastText& model
    , FastTextEmbedder::AggregationMode mode
    , size_t maxWords
    , const std::string& matrixPath
    , const std::string& biasPath
)
    : Model(model)
    , Mode(mode)
    , MaxWords(maxWords)
    , Matrix(model.getDimension() * 3, 50)
    , Bias(50)
{
    if (matrixPath.empty()) {
        return;
    }
    std::ifstream matrixIn(matrixPath);
    std::string line;
    int row = 0;
    int col = 0;
    while (std::getline(matrixIn, line)) {
        std::string num;
        for (char ch : line) {
            if (ch == ' ') {
                continue;
            }
            if (ch != ',' && ch != '\n') {
                num += ch;
                continue;
            }
            Matrix(col++, row) = std::stof(num);
            num = "";
        }
        if (!num.empty()) {
            Matrix(col, row) = std::stof(num);
            num = "";
        }
        col = 0;
        row++;
    }
    matrixIn.close();
    assert(row != 0);
    assert(row == 50);

    std::ifstream biasIn(biasPath);
    row = 0;
    while (std::getline(biasIn, line)) {
        Bias(row++) = std::stof(line);
    }
    biasIn.close();
}

size_t FastTextEmbedder::GetEmbeddingSize() const {
    return Model.getDimension();
}

fasttext::Vector FastTextEmbedder::GetSentenceEmbedding(const Document& doc) const {
    std::istringstream ss(doc.Title + " " + doc.Text);
    fasttext::Vector wordVector(FastTextEmbedder::GetEmbeddingSize());
    fasttext::Vector avgVector(FastTextEmbedder::GetEmbeddingSize());
    fasttext::Vector maxVector(FastTextEmbedder::GetEmbeddingSize());
    fasttext::Vector minVector(FastTextEmbedder::GetEmbeddingSize());
    std::string word;
    size_t count = 0;
    while (ss >> word) {
        if (count > MaxWords) {
            break;
        }
        Model.getWordVector(wordVector, word);
        float norm = wordVector.norm();
        if (norm < 0.0001) {
            continue;
        }
        wordVector.mul(1.0 / norm);

        avgVector.addVector(wordVector);
        if (count == 0) {
            maxVector = wordVector;
            minVector = wordVector;
        } else {
            for (size_t i = 0; i < FastTextEmbedder::GetEmbeddingSize(); i++) {
                maxVector[i] = std::max(maxVector[i], wordVector[i]);
                minVector[i] = std::min(minVector[i], wordVector[i]);
            }
        }
        count += 1;
    }
    if (count > 0) {
        avgVector.mul(1.0 / count);
    }
    if (Mode == AM_Avg) {
        return avgVector;
    } else if (Mode == AM_Min) {
        return minVector;
    } else if (Mode == AM_Max) {
        return maxVector;
    }
    assert(Mode == AM_Matrix);
    fasttext::Vector resultVector(FastTextEmbedder::GetEmbeddingSize());
    Eigen::VectorXf concatVector(FastTextEmbedder::GetEmbeddingSize() * 3);
    for (size_t i = 0; i < FastTextEmbedder::GetEmbeddingSize(); i++) {
        concatVector[i] = avgVector[i];
        concatVector[FastTextEmbedder::GetEmbeddingSize() + i] = maxVector[i];
        concatVector[2 * FastTextEmbedder::GetEmbeddingSize() + i] = minVector[i];
    }
    auto eigenResult = ((Matrix.transpose() * concatVector) + Bias).transpose();
    for (size_t i = 0; i < FastTextEmbedder::GetEmbeddingSize(); i++) {
        resultVector[i] = eigenResult(0, i);
    }
    return resultVector;
}