
#include "main.h"

TestExample TestFramework::makeImageSmoothing(const string &imageFilename, float w)
{
    //
    // terms:
    // smoothness: 4 * x_i - (neighbors) = 0
    // reconstruction: x_i = c_i
    // 
    // final energy function:
    // E(x) = sum_i( (4 * x_i - (neighbors) ) ^2 ) + sum_i( w * (x_i - target_i)^2 )
    //
    // minimized with A = L^T L + I * w, b = I * w * target

    const Bitmap bmp = LodePNG::load(imageFilename);
    const int dimX = bmp.getWidth();
    const int dimY = bmp.getHeight();

    const size_t pixelCount = bmp.size();

    auto getVariable = [=](size_t x, size_t y)
    {
        return (size_t)(y * dimX + x);
    };

    auto isBorder = [=](size_t x, size_t y)
    {
        return (x == 0 || y == 0 || x == dimX - 1 || y == dimY - 1);
    };

    SparseMatrixf L(pixelCount, pixelCount);
    for (const auto &p : bmp)
    {
        if (isBorder(p.x, p.y))
            continue;

        size_t row = getVariable(p.x, p.y);
        L(row, row) = 4.0;
        L(row, getVariable(p.x - 1, p.y + 0)) = -1.0;
        L(row, getVariable(p.x + 1, p.y + 0)) = -1.0;
        L(row, getVariable(p.x + 0, p.y - 1)) = -1.0;
        L(row, getVariable(p.x + 0, p.y + 1)) = -1.0;
    }

    MathVector<float> targetValues(pixelCount);
    for (const auto &p : bmp)
        targetValues[getVariable(p.x, p.y)] = p.value.r;

    SparseMatrixf W = SparseMatrixf::identity(pixelCount) * w;

    SparseMatrixf A = L.transpose() * L + W;
    MathVector<float> b = W * targetValues;

    LinearSolverConjugateGradient<float> solver;
    MathVector<float> x = solver.solve(A, b);

    Bitmap testImage = bmp;
    for (const auto &p : bmp)
        testImage(p.x, p.y) = vec4uc(util::boundToByte(x[getVariable(p.x, p.y)]));

    LodePNG::save(testImage, "smoothingOutputLinearSolve.png");

    TestExample result("imageSmoothing", "imageSmoothingAD.t", bmp.getWidth(), bmp.getHeight());

    result.costFunction = [=](const OptImage &variables)
    {
        //(4 * x_i - (neighbors) ) ^2 ) + sum_i( w * (x_i - target_i)^2
        float sum = 0.0;

        //
        // Laplacian cost
        //
        for (const auto &p : bmp)
        {
            if (isBorder(p.x, p.y))
                continue;

            const float x = variables(p.x, p.y);

            const float n0 = variables(p.x - 1, p.y);
            const float n1 = variables(p.x + 1, p.y);
            const float n2 = variables(p.x, p.y - 1);
            const float n3 = variables(p.x, p.y + 1);

            const float laplacianCost = 4 * x - (n0 + n1 + n2 + n3);

            sum += laplacianCost * laplacianCost;
        }

        //
        // Reconstruction cost
        //
        for (const auto &p : bmp)
        {
            const float x = variables(p.x, p.y);
            const float reconstructionCost = x - p.value.r;

            sum += w * (reconstructionCost * reconstructionCost);
        }
        
        return sum;
    };

    result.images.resize(2);
    result.images[0].allocate(bmp.getWidth(), bmp.getHeight());
    result.images[1].allocate(bmp.getWidth(), bmp.getHeight());
    result.minimumValues = result.images[0];

    for (const auto &p : bmp)
    {
        result.images[0](p.x, p.y) = 0.0;
        result.images[1](p.x, p.y) = p.value.r;
        result.minimumValues(p.x, p.y) = x[getVariable((int)p.x, (int)p.y)];
    }

    result.minimumCost = result.costFunction(result.minimumValues);
    

    return result;
}

TestExample TestFramework::makeRandomQuadratic(int count)
{
    TestExample result("quadratic1D", "quadratic.t", count, 1);
    
    //
    // image order: x, a, b, c
    //
    result.images.resize(4);
    for (auto &image : result.images)
        image.allocate(count, 1);

    for (int i = 0; i < count; i++)
    {
        result.images[1](i, 0) = i * 0.1f + 1.0f;
        result.images[2](i, 0) = i * 0.1f + 2.0f;
        result.images[3](i, 0) = i * 0.1f + 3.0f;
    }

    //
    // residual_i = ( (ax^2 + bx + c)^2 = 0)
    //

    result.costFunction = [=](const OptImage &variables)
    {
        float sum = 0.0;
        for (int i = 0; i < count; i++)
        {
            const float x = variables(i, 0);
            const float a = result.images[1](i, 0);
            const float b = result.images[2](i, 0);
            const float c = result.images[3](i, 0);
            const float v = a * x * x + b * x + c;
            sum += v;
        }
        return sum;
    };

    float minimumSum = 0.0f;
    for (int i = 0; i < count; i++)
    {
        const float a = result.images[1](i, 0);
        const float b = result.images[2](i, 0);
        const float c = result.images[3](i, 0);
        const float x = -b / (2.0f * a);
        const float v = a * x * x + b * x + c;
        minimumSum += v;
    }
    result.minimumCost = minimumSum;

    return result;
}

void TestFramework::runAllTests()
{
    optimizerState = Opt_NewState();
    if (optimizerState == nullptr)
    {
        cerr << "Opt_NewState failed" << endl;
        return;
    }

    //TestExample example = makeRandomQuadratic(5);
    TestExample example = makeImageSmoothing("smoothingExampleB.png", 0.1f);

    vector<TestMethod> methods;

    //
    // CPU methods
    //
    //methods.push_back(TestMethod("gradientDescentCPU","no-params"));
    //methods.push_back(TestMethod("conjugateGradientCPU", "no-params"));
    //methods.push_back(TestMethod("linearizedConjugateGradientCPU", "no-params"));
    //methods.push_back(TestMethod("lbfgsCPU", "no-params"));
    //methods.push_back(TestMethod("vlbfgsCPU", "no-params"));
    methods.push_back(TestMethod("bidirectionalVLBFGSCPU", "no-params"));

    //
    // GPU methods
    //
    //methods.push_back(TestMethod("gradientDescentGPU", "no-params"));
    //methods.push_back(TestMethod("vlbfgsGPU", "no-params"));

    for (auto &method : methods)
        runTest(method, example);
}

void TestFramework::runTest(const TestMethod &method, TestExample &example)
{
    example.images[0].clear(0.0f);

    cout << "Running test: " << example.exampleName << " using " << method.optimizerName << endl;

    cout << "start cost: " << example.costFunction(example.images[0]) << endl;

    uint64_t dims[] = { example.variableDimX, example.variableDimY };

    Problem * prob = Opt_ProblemDefine(optimizerState, example.terraCodeFilename.c_str(), method.optimizerName.c_str(), NULL);

    if (!prob)
    {
        cout << "Opt_ProblemDefine failed" << endl;
        cin.get();
        return;
    }
    
    std::vector<void*> imagesCPU;
    std::vector<void*> imagesGPU;
    std::vector<uint64_t> stride;
    std::vector<uint64_t> elemsize;
    for (const auto &image : example.images)
    {
        image.syncCPUToGPU();
        imagesCPU.push_back((void*)image.DataCPU());
        imagesGPU.push_back((void*)image.DataGPU());
        stride.push_back(image.dimX * sizeof(float));
        elemsize.push_back(sizeof(float));
    }
    
    Plan * plan = Opt_ProblemPlan(optimizerState, prob, dims, elemsize.data(), stride.data());

    if (!plan)
    {
        cout << "Opt_ProblemPlan failed" << endl;
        cin.get();
        return;
    }

    bool isGPU = ml::util::endsWith(method.optimizerName, "GPU");

    if (isGPU)
    {
        Opt_ProblemSolve(optimizerState, plan, imagesGPU.data(), NULL);
        for (const auto &image : example.images)
            image.syncGPUToCPU();
    }
    else
        Opt_ProblemSolve(optimizerState, plan, imagesCPU.data(), NULL);

    //cout << "x(0, 0) = " << example.images[0](0, 0) << endl;
    //cout << "x(1, 0) = " << example.images[0](5, 0) << endl;
    //cout << "x(0, 1) = " << example.images[0](0, 5) << endl;



    // TODO: this is not always accurate, in cases where costFunction does not exactly match the cost function in the terra file.  This should just call terra's cost function.
    cout << "optimized cost: " << example.costFunction(example.images[0]) << endl;

    cout << "expected cost: " << example.minimumCost << endl;

    cout << "max delta: " << OptImage::maxDelta(example.images[0], example.minimumValues) << endl;
    cout << "avg delta: " << OptImage::avgDelta(example.images[0], example.minimumValues) << endl;
}
