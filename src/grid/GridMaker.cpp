#include "GridMaker.h"
#include "utilities/Parallel.h"
#include "utilities/Utils.h"
#include "CellData.h"
#include "GridBox.h"

#include <iostream>

/*
 * How to make a grid? Easy!
 * - setup physical params: size, macro values, etc
 * - normalize data
 *
 * MPI workflow:
 * - main process load all configs, splits them for each process, add's special cells
 * - slave process just loads configs and that's it...
 */
Grid<CellData>* GridMaker::makeGrid(const Vector2u& size) {
    Grid<CellData>* grid = nullptr;

    if (Parallel::isUsingMPI() == true && Parallel::getSize() > 1) {
        if (Parallel::isMaster() == true) {

            // make configs for whole task
            Grid<CellData>* wholeGrid = makeWholeGrid(size);

            std::cout << "Whole grid:" << std::endl;
            std::cout << *wholeGrid << std::endl;

            // split configs onto several parts, for each process
            std::vector<Grid<CellData>*> splittedGrids = splitGrid(wholeGrid, (unsigned int) Parallel::getSize());

            std::cout << "Splitted grids:" << std::endl;
            for (unsigned int y = 0; y < wholeGrid->getSize().y(); y++) {
                for (unsigned int i = 0; i < splittedGrids.size(); i++) {
                    Grid<CellData>* splitGrid = splittedGrids[i];
                    for (unsigned int x = 0; x < splitGrid->getSize().x(); x++) {
                        CellData* data = splitGrid->get(x, y);

                        char code = 'X';
                        if (data == nullptr) {
                            code = ' ';
                        } else if (data->getType() == CellData::Type::FAKE) {
                            code = '1';
                        } else if (data->getType() == CellData::Type::NORMAL) {
                            code = '0';
                        } else if (data->getType() == CellData::Type::FAKE_PARALLEL) {
                            code = 'P';
                        }

                        std::cout << code;
                    }
                    if (i != splittedGrids.size() - 1) {
                        std::cout << " ";
                    }
                }
                std::cout << std::endl;
            }

            // Self configs
            grid = splittedGrids[0];

            // Send to other processes
            for (int processor = 1; processor < Parallel::getSize(); processor++) {
                const char* buffer = Utils::serialize(splittedGrids[processor]);
                Parallel::send(buffer, processor, Parallel::COMMAND_GRID);
            }
        } else {
            const char* buffer = Parallel::recv(0, Parallel::COMMAND_GRID);
            Utils::deserialize(buffer, grid);
        }
    } else {
        grid = makeWholeGrid(size);
        std::cout << *grid << std::endl;
    }

    return grid;
}

Grid<CellData>* GridMaker::makeWholeGrid(const Vector2u& size) {

    // create boxes
    std::vector<GridBox*> boxes = makeBoxes();

    // find cells field size
    Vector2d lbPoint = boxes.front()->getPoint();
    Vector2d rtPoint = lbPoint;
    for (GridBox *box : boxes) {
        const Vector2d &boxPoint = box->getPoint();
        const Vector2d &boxSize = box->getSize();
        if (boxPoint.x() < lbPoint.x()) {
            lbPoint.x() = boxPoint.x();
        }
        if (boxPoint.y() < lbPoint.y()) {
            lbPoint.y() = boxPoint.y();
        }
        if (boxPoint.x() + boxSize.x() > rtPoint.x()) {
            rtPoint.x() = boxPoint.x() + boxSize.x();
        }
        if (boxPoint.y() + boxSize.y() > rtPoint.y()) {
            rtPoint.y() = boxPoint.y() + boxSize.y();
        }
    }
    Vector2d wholeSize = rtPoint - lbPoint; // in original data

    // create configs for whole space
    Grid<CellData>* grid = new Grid<CellData>(size);
    for (auto box : boxes) {
        const Vector2d lbWholeBoxPoint = box->getPoint() - lbPoint;
        const Vector2d rtWholeBoxPoint = lbWholeBoxPoint + box->getSize();

        Vector2u lbWholeBoxGridPoint = {
                (unsigned int) (lbWholeBoxPoint.x() / wholeSize.x() * (size.x() - 2)) + 1,
                (unsigned int) (lbWholeBoxPoint.y() / wholeSize.y() * (size.y() - 2)) + 1
        };

        Vector2u rtWholeBoxGridPoint = {
                (unsigned int) (rtWholeBoxPoint.x() / wholeSize.x() * (size.x() - 2)) + 1,
                (unsigned int) (rtWholeBoxPoint.y() / wholeSize.y() * (size.y() - 2)) + 1
        };

        if (box->isSolid() == false) {
            lbWholeBoxGridPoint.x() += -1;
            lbWholeBoxGridPoint.y() += -1;
            rtWholeBoxGridPoint.x() += 1;
            rtWholeBoxGridPoint.y() += 1;
        }

        const Vector2u wholeBoxGridSize = rtWholeBoxGridPoint - lbWholeBoxGridPoint;

        for (unsigned int x = 0; x < size.x(); x++) {
            for (unsigned int y = 0; y < size.y(); y++) {
                if (box->isSolid() == false) {
                    Vector2d point{
                            1.0 * (x - lbWholeBoxGridPoint.x()) / (rtWholeBoxGridPoint.x() - lbWholeBoxGridPoint.x() - 1),
                            1.0 * (y - lbWholeBoxGridPoint.y()) / (rtWholeBoxGridPoint.y() - lbWholeBoxGridPoint.y() - 1)
                    };

                    if (x > lbWholeBoxGridPoint.x() && x < rtWholeBoxGridPoint.x() - 1 && y == lbWholeBoxGridPoint.y()) {
                        auto data = new CellData(CellData::Type::FAKE);
                        if (box->getBottomBorderFunction() != nullptr) {
                            box->getBottomBorderFunction()(point.x(), *data);
                        }
                        grid->set(x, y, data);
                    } else if (x > lbWholeBoxGridPoint.x() && x < rtWholeBoxGridPoint.x() - 1 && y == rtWholeBoxGridPoint.y() - 1) {
                        auto data = new CellData(CellData::Type::FAKE);
                        if (box->getTopBorderFunction() != nullptr) {
                            box->getTopBorderFunction()(point.x(), *data);
                        }
                        grid->set(x, y, data);
                    } else if (x == lbWholeBoxGridPoint.x() && y > lbWholeBoxGridPoint.y() && y < rtWholeBoxGridPoint.y() - 1) {
                        auto data = new CellData(CellData::Type::FAKE);
                        if (box->getLeftBorderFunction() != nullptr) {
                            box->getLeftBorderFunction()(point.y(), *data);
                        }
                        grid->set(x, y, data);
                    } else if (x == rtWholeBoxGridPoint.x() - 1 && y > lbWholeBoxGridPoint.y() && y < rtWholeBoxGridPoint.y() - 1) {
                        auto data = new CellData(CellData::Type::FAKE);
                        if (box->getRightBorderFunction() != nullptr) {
                            box->getRightBorderFunction()(point.y(), *data);
                        }
                        grid->set(x, y, data);
                    } else if (x > lbWholeBoxGridPoint.x() && x < rtWholeBoxGridPoint.x() - 1 && y > lbWholeBoxGridPoint.y() && y < rtWholeBoxGridPoint.y() - 1) {
                        auto data = new CellData(CellData::Type::NORMAL);
                        if (box->getMainFunction() != nullptr) {
                            box->getMainFunction()(point, *data);
                        }
                        grid->set(x, y, data);
                    }
                } else {
                    Vector2d point{
                            1.0 * (x - lbWholeBoxGridPoint.x()) / (rtWholeBoxGridPoint.x() - lbWholeBoxGridPoint.x() - 1),
                            1.0 * (y - lbWholeBoxGridPoint.y()) / (rtWholeBoxGridPoint.y() - lbWholeBoxGridPoint.y() - 1)
                    };

                    if (x > lbWholeBoxGridPoint.x() && x < rtWholeBoxGridPoint.x() - 1 && y == lbWholeBoxGridPoint.y()) {
                        auto data = new CellData(CellData::Type::FAKE);
                        if (box->getBottomBorderFunction() != nullptr) {
                            box->getBottomBorderFunction()(point.x(), *data);
                        }
                        grid->set(x, y, data);
                    } else if (x > lbWholeBoxGridPoint.x() && x < rtWholeBoxGridPoint.x() - 1 && y == rtWholeBoxGridPoint.y() - 1) {
                        auto data = new CellData(CellData::Type::FAKE);
                        if (box->getTopBorderFunction() != nullptr) {
                            box->getTopBorderFunction()(point.x(), *data);
                        }
                        grid->set(x, y, data);
                    } else if (x == lbWholeBoxGridPoint.x() && y > lbWholeBoxGridPoint.y() && y < rtWholeBoxGridPoint.y() - 1) {
                        auto data = new CellData(CellData::Type::FAKE);
                        if (box->getLeftBorderFunction() != nullptr) {
                            box->getLeftBorderFunction()(point.y(), *data);
                        }
                        grid->set(x, y, data);
                    } else if (x == rtWholeBoxGridPoint.x() - 1 && y > lbWholeBoxGridPoint.y() && y < rtWholeBoxGridPoint.y() - 1) {
                        auto data = new CellData(CellData::Type::FAKE);
                        if (box->getRightBorderFunction() != nullptr) {
                            box->getRightBorderFunction()(point.y(), *data);
                        }
                        grid->set(x, y, data);
                    } else if (x == lbWholeBoxGridPoint.x() && y == lbWholeBoxGridPoint.y()) {
                        auto data = new CellData(CellData::Type::FAKE);
                        if (box->getBottomBorderFunction() != nullptr) {
                            box->getBottomBorderFunction()(point.x(), *data);
                        }
                        if (box->getLeftBorderFunction() != nullptr) {
                            box->getLeftBorderFunction()(point.y(), *data);
                        }
                        grid->set(x, y, data);
                    } else if (x == lbWholeBoxGridPoint.x() && y == rtWholeBoxGridPoint.y() - 1) {
                        auto data = new CellData(CellData::Type::FAKE);
                        if (box->getLeftBorderFunction() != nullptr) {
                            box->getLeftBorderFunction()(point.y(), *data);
                        }
                        if (box->getTopBorderFunction() != nullptr) {
                            box->getTopBorderFunction()(point.x(), *data);
                        }
                        grid->set(x, y, data);
                    } else if (x == rtWholeBoxGridPoint.x() - 1 && y == lbWholeBoxGridPoint.y()) {
                        auto data = new CellData(CellData::Type::FAKE);
                        if (box->getBottomBorderFunction() != nullptr) {
                            box->getBottomBorderFunction()(point.x(), *data);
                        }
                        if (box->getRightBorderFunction() != nullptr) {
                            box->getRightBorderFunction()(point.y(), *data);
                        }
                        grid->set(x, y, data);
                    } else if (x == rtWholeBoxGridPoint.x() - 1 && y == rtWholeBoxGridPoint.y() - 1) {
                        auto data = new CellData(CellData::Type::FAKE);
                        if (box->getTopBorderFunction() != nullptr) {
                            box->getTopBorderFunction()(point.x(), *data);
                        }
                        if (box->getRightBorderFunction() != nullptr) {
                            box->getRightBorderFunction()(point.y(), *data);
                        }
                        grid->set(x, y, data);
                    } else if (x > lbWholeBoxGridPoint.x() && x < rtWholeBoxGridPoint.x() - 1 && y > lbWholeBoxGridPoint.y() && y < rtWholeBoxGridPoint.y() - 1) {
                        grid->set(x, y, nullptr);
                    }
                }
            }
        }
    }

    return grid;
}

std::vector<GridBox*> GridMaker::makeBoxes() {
    std::vector<GridBox*> boxes;

    GridBox* box = nullptr;

    box = new GridBox(Vector2d(0.0, 0.0), Vector2d(100.0, 100.0), false);
    box->setMainFunction([](Vector2d point, CellData& data) {
        data.params().set(0, 1.0, 1.0, 1.0);
        data.setStep(Vector3d(0.1, 0.1, 0.0));
    });
    box->setLeftBorderFunction([](double point, CellData& data) {
        data.setBoundaryTypes(0, CellData::BoundaryType::DIFFUSE);
        data.boundaryParams().setTemp(0, 1.0);
        data.setStep(Vector3d(0.1, 0.1, 0.0));
    });
    box->setRightBorderFunction([](double point, CellData& data) {
        data.setBoundaryTypes(0, CellData::BoundaryType::DIFFUSE);
        data.boundaryParams().setTemp(0, 1.0);
        data.setStep(Vector3d(0.1, 0.1, 0.0));
    });
    box->setTopBorderFunction([](double point, CellData& data) {
        data.setBoundaryTypes(0, CellData::BoundaryType::DIFFUSE);
        data.boundaryParams().setTemp(0, 1.0);
        data.setStep(Vector3d(0.1, 0.1, 0.0));
    });
    box->setBottomBorderFunction([](double point, CellData& data) {
        data.setBoundaryTypes(0, CellData::BoundaryType::DIFFUSE);
        data.boundaryParams().setTemp(0, 1.0);
        data.setStep(Vector3d(0.1, 0.1, 0.0));
    });
    boxes.push_back(box);

    box = new GridBox(Vector2d(30.0, 30.0), Vector2d(40.0, 40.0), true);
    box->setLeftBorderFunction([](double point, CellData& data) {
        data.setBoundaryTypes(0, CellData::BoundaryType::DIFFUSE);
        data.boundaryParams().setTemp(0, 2.0);
        data.setStep(Vector3d(0.1, 0.1, 0.0));
    });
    box->setRightBorderFunction([](double point, CellData& data) {
        data.setBoundaryTypes(0, CellData::BoundaryType::DIFFUSE);
        data.boundaryParams().setTemp(0, 2.0);
        data.setStep(Vector3d(0.1, 0.1, 0.0));
    });
    box->setTopBorderFunction([](double point, CellData& data) {
        data.setBoundaryTypes(0, CellData::BoundaryType::DIFFUSE);
        data.boundaryParams().setTemp(0, 2.0);
        data.setStep(Vector3d(0.1, 0.1, 0.0));
    });
    box->setBottomBorderFunction([](double point, CellData& data) {
        data.setBoundaryTypes(0, CellData::BoundaryType::DIFFUSE);
        data.boundaryParams().setTemp(0, 2.0);
        data.setStep(Vector3d(0.1, 0.1, 0.0));
    });
    boxes.push_back(box);

    return boxes;
}

std::vector<Grid<CellData>*> GridMaker::splitGrid(Grid<CellData>* grid, unsigned int numGrids) {
    std::vector<Grid<CellData>*> grids;

    unsigned int countNotNull = grid->getCountNotNull();
    unsigned int splitCount = countNotNull / numGrids;
    unsigned int splitIndex = 0;
    unsigned int indexNotNull = 0;
    unsigned int lastIndex = 0;
    for (unsigned int index = 0; index < grid->getCount(); index++) {
        auto data = grid->getByIndex(index);
        if (data != nullptr) {
            indexNotNull++;
        }

        if (indexNotNull == splitCount * (splitIndex + 1) && indexNotNull <= countNotNull - splitCount
            || indexNotNull == countNotNull && splitIndex < numGrids) {

            // get new grid size and shift on X axis
            unsigned int shiftIndex = lastIndex - lastIndex % grid->getSize().y();
            unsigned int sizeX = Grid<CellData>::toPoint(index, grid->getSize()).x() - Grid<CellData>::toPoint(lastIndex, grid->getSize()).x() + 1;

            // make split here
            Grid<CellData>* newGrid = new Grid<CellData>(grid->getSize());
            for (unsigned int k = lastIndex; k <= index; k++) {
                unsigned int newIndex = k - shiftIndex;
                newGrid->setByIndex(newIndex, grid->getByIndex(k));
            }

            // add 1 column to left
            if (splitIndex > 0) {
                sizeX += 1;
                shiftIndex -= grid->getSize().y();
                newGrid->resize(Vector2i(-1, 0), newGrid->getSize() - Vector2u(1, 0));
                for (unsigned int k = lastIndex - grid->getSize().y(); k < lastIndex; k++) {
                    auto* kData = grid->getByIndex(k);
                    if (kData != nullptr && kData->isFake() == false) {
                        auto newData = new CellData(*kData);
                        newData->setType(CellData::Type::FAKE_PARALLEL);
                        newData->setIndexInOriginalGrid(k);
                        newData->setProcessorOfOriginalGrid(splitIndex - 1);

                        unsigned int newIndex = k - shiftIndex;
                        newGrid->setByIndex(newIndex, newData);
                    }
                }
            }

            // add 2 column to right
            if (splitIndex < numGrids - 1) {
                sizeX += 2;
                for (unsigned int k = index + 1; k < index + 1 + grid->getSize().y() * 2; k++) {
                    auto* kData = grid->getByIndex(k);
                    if (kData != nullptr && kData->isFake() == false) {
                        auto newData = new CellData(*kData);
                        newData->setType(CellData::Type::FAKE_PARALLEL);
                        newData->setIndexInOriginalGrid(k);
                        newData->setProcessorOfOriginalGrid(splitIndex + 1);

                        unsigned int newIndex = k - shiftIndex;
                        newGrid->setByIndex(newIndex, newData);
                    }
                }
            }

            // cut unused space
            newGrid->resize(Vector2i(), Vector2u(sizeX, grid->getSize().y()));
            grids.push_back(newGrid);

            lastIndex = index + 1;
            splitIndex++;
        }
    }

    return grids;
}

void GridMaker::syncGrid(Grid<Cell>* grid) {

    // need vectors of ids
    std::vector<int> sendNextIds;
    std::vector<int> sendPrevIds;
    std::vector<int> recvNextIds;
    std::vector<int> recvPrevIds;

    int processor = Parallel::getRank();
    int size = Parallel::getSize();

    // fill send vectors
    for (auto* cell : grid->getValues()) {
        if (cell != nullptr && cell->getData()->getType() == CellData::Type::FAKE_PARALLEL) {
            if (cell->getData()->getProcessorOfOriginalGrid() > processor) {
                sendNextIds.push_back(cell->getData()->getIndexInOriginalGrid());
            } else {
                sendPrevIds.push_back(cell->getData()->getIndexInOriginalGrid());
            }
        }
    }

    // parallel exchange for ids
    if (processor % 2 == 0) {
        if (processor < size - 1) {
            Parallel::send(Utils::serialize(sendNextIds), processor + 1, Parallel::COMMAND_SYNC_IDS);
            Utils::deserialize(Parallel::recv(processor + 1, Parallel::COMMAND_SYNC_IDS), recvNextIds);
        }
        if (processor > 0) {
            Utils::deserialize(Parallel::recv(processor - 1, Parallel::COMMAND_SYNC_IDS), recvPrevIds);
            Parallel::send(Utils::serialize(sendPrevIds), processor - 1, Parallel::COMMAND_SYNC_IDS);
        }
    } else {
        if (processor > 0) {
            Utils::deserialize(Parallel::recv(processor - 1, Parallel::COMMAND_SYNC_IDS), recvPrevIds);
            Parallel::send(Utils::serialize(sendPrevIds), processor - 1, Parallel::COMMAND_SYNC_IDS);
        }
        if (processor < size - 1) {
            Parallel::send(Utils::serialize(sendNextIds), processor + 1, Parallel::COMMAND_SYNC_IDS);
            Utils::deserialize(Parallel::recv(processor + 1, Parallel::COMMAND_SYNC_IDS), recvNextIds);
        }
    }
}
