# include "RandomForest.h"

/**
 * Thread function that test a number of split candidates and return the
 * best.
 */
 //CHECK
void* findSplitThread (void* args)

{
    RandomForest& f = *(((SCParams*) args) -> forest);
    return new SplitCandidate(f.bestSplitCandidate(*((SCParams*)args)));
}

/**
 * Random forest empty constructor
 */
 //CHECK
RandomForest :: RandomForest () 
{
    tp = NULL;
}

/**
 * Calculates the feature function given the offsets and the
 * pixel.
 *
 * @param u first pixel offset.
 * @param v second pixel offset.
 * @param x Pixel coordinate of the pixel for the feature to be
 * calculated.
 *
 * @return value of the calculated feature.
 */
 //CHECK
float RandomForest :: calcFeature(Offset u,
                                  Offset v, 
                                  PixelInfo x,
                                  Image *img)
{

    PixelInfo xUoff;
    PixelInfo xVoff;
    unsigned dx;
    unsigned uDepth;
    unsigned vDepth;
    
    dx = img -> getDepth(x.x, x.y);

    // Normalize offsets by the depth at pixel
    u /= dx;
    v /= dx;

    xUoff = x + u;
    xVoff = x + v;
    
    uDepth = img -> getDepth(xUoff.x, xUoff.y);
    vDepth = img -> getDepth(xVoff.x, xVoff.y);

    return uDepth - vDepth;
    
}

/** 
 * sortData
 *
 * Sorts the training data array and returns the index
 * which splits the  array into left and right sets.
 *
 * @param range of the train set.
 * @param f feature that corresponds to the best split of the
 * training data
 *
 * @return index that defines the best split of the array. The index
 * is placed in the first element of the right set.
 */
 //CHECK
int RandomForest :: sortData (NumRange range, SplitCandidate f)
{
    int i;
    unsigned pivot;
    bool findRight;
    pixelSet ps;
    PixelInfo temp;
    TrainImage* imgPtr;
    
    findRight = false;

    for (i = range.start; i <= range.end; i++){
 
        // Get the image pointer from imagePool with id of pixel.
        imgPtr = images -> getImgPtr((*td)[i].id);
        ps = classifyPixel(f, (*td)[i], (Image*)imgPtr);
        
        if ((findRight == false) && (ps == RIGHT)) {
            findRight = true;
            pivot = i;
        } 
        else if ((findRight == true) && (ps == LEFT)) {
            temp = (*td)[pivot];
            (*td)[pivot] = (*td)[i];
            (*td)[i] = temp;
            pivot++;
        }
    }

    return pivot;
}

/**
 *  bestSplitThreadFun
 *
 *  This function runs the bestSplitCandidate in diferent
 *  threads. It divides the work between them by the macro
 *  THREADS_PER_NODE.
 *
 *  @param structure with the parameter to generate a split
 *  candidate.
 *
 *  @return The best split candidate generated
 */
 //CHECK
SplitCandidate RandomForest :: bestSplitThreadFun(NumRange range)
{

    int i;
    int errCode;
    pthread_t threads[THREADS_PER_NODE];
    SplitCandidate bestSplit;
    SplitCandidate* sc;
    SCParams params;

    params.forest = this;
    params.trainDataRange = range;

    for (i = 0; i < THREADS_PER_NODE; i++) {

        if ((errCode = pthread_create(&threads[i],
                        NULL,                    
                        findSplitThread,
                        &params))) {
            printf("We must pay for our arrogance\n");
        }
    }

    bestSplit = SplitCandidate();

    for (i = 0; i < THREADS_PER_NODE; i++) {
        if ((errCode = pthread_join(threads[i], (void **) &sc))) { 
            printf("We must pay for our arrogance\n");
        }

        if (sc -> g > bestSplit.g) {
            bestSplit = *sc;
        }
    }

    return bestSplit;
}

/**
 *  bestSplitCandidate
 *
 *  This function generate and return a feature that maximise
 *  the function G. This function create split candidates
 *  randomly and test them in the train data. The best feature
 *  generated will be return.
 *
 *  @param a structure with the information to generate a split
 *  candidate.
 *
 *  @return the best split candidate generated.
 */
//CHECK
SplitCandidate RandomForest :: bestSplitCandidate(SCParams& params)
{

    unsigned offsetNum = tp -> offsetNum / THREADS_PER_NODE;
    unsigned thresholdNum = tp -> thresholdNum / THREADS_PER_NODE;
    NumRange trainDataRange = params.trainDataRange;

    unsigned i;
    unsigned j;

    Offset u;
    Offset v;
    float t;

    float setEntropy;
    float gain;
    float bestGain;
    SplitCandidate phi;
    SplitCandidate bestPhi;

    // Initialize information gain
    bestGain = 0.0f;

    // Get the entropy of the entire set
    setEntropy = calcSetEntropy(trainDataRange);

    // Start generating and testing the features.
    for (i = 0; i < offsetNum; i++) {

        // Generate a pair of random offsets
        u = Offset(tp -> offsetRange);
        v = Offset(tp -> offsetRange);
        
        for (j = 0; j < thresholdNum; j++) {
       
            // Generate random threshold
            t = randFloat(tp -> thresholdRange);

            //Generate the feature to test
            phi = SplitCandidate (u, v, t);
            
            // Get phi information gain
            gain = G(phi, setEntropy, trainDataRange);

            // Keep the feature if it has a good information gain
            if (gain > bestGain) {
                bestPhi = phi;
                bestPhi.g = gain;
                bestGain = gain;
            }
        }
    }
    return bestPhi;
}

/**
 *  calcSetEntropy
 *
 *  This function calculate the entropy of the train set data
 *  between the begin and end index parameters.
 *
 *  @parma begin is the index from where to start the entropy
 *  calculation.
 *  @param end is the index where to stop the entropy
 *  calculation.
 *
 *  @return the shannon entropy of the train data set between
 *  the begin and end index parameters.
 *
 */
//CHECK
float RandomForest :: calcSetEntropy(NumRange& setRange)
{
    int i;
    Label label;
    vector <float> percentages;

    // Initialize percentage vector.
    percentages.resize(tp -> labelNum, 0.0f);

    // Calculate the sum of the labels
    for (i = setRange.start; i <= setRange.end; i++) {
        label = images -> getLabel((*td)[i]);

        percentages[label - 1] += 1.0f;
    }
    

    // Calculate the percentage for each label
    for (i = 0; i < tp -> labelNum; i++) {
        percentages[i] /= (setRange.end - setRange.start + 1);
    }
    
    return H(percentages);
}

/**
 *  Information Gain
 *
 *  This function returns the total of information gain resulting 
 *  after spliting the data set by the feature phi between the begin
 *  and end index parameters.
 *
 *  @param phi is the SplitCandidate to evalute its entropy
 *  @param setEntropy is the total entropy of the set
 *  @param begin is the index of the data set to start the
 *  entropy calculation.
 *  @param end is the index of the data set to stop the
 *  entropy calculation.
 *
 *  @return the total entropy of the phi feature
 *
 */
//CHECK
float RandomForest :: G(SplitCandidate phi, 
                        float setEntropy, 
                        NumRange setRange)
{
    float gain;
    vector <float> left;
    vector <float> right;
    unsigned leftCard;
    unsigned rightCard;

    // Calculate the split percentage
    getPercentage(phi, 
                  left, 
                  right, 
                  leftCard,
                  rightCard, 
                  setRange); 

    gain = setEntropy;
    gain -= ((double)leftCard / (double)(leftCard + rightCard)) * H(left);
    gain -= ((double)rightCard / (double)(leftCard + rightCard)) * H(right);

    return gain;
}


/**
 *  getPercentage
 *
 *  This function apply the SplitCandidate phi to the TrainData set between the
 *  begin and end index parameters. The percentage of the labels
 *  ocurrences are divided in two vector left and right. Each of this
 *  vectors contains the percentage of each label in each set.
 *  
 *  @param phi is the feature to be applyed to the trainData set.
 *  @param left is a vector that will contain the percentage of each
 *  label in the left splited set by phi.
 *  @param right is a vector that will contain the percentage of each
 *  label in the right splited set by phi.
 *  @param leftCard is the cardinality of the resulting left set.
 *  @param rightCard is the cardinality of the resulting right set.
 *  @param range of the train set.
 *
 */
//CHECK
void RandomForest :: getPercentage(SplitCandidate phi,
                                   vector <float>& left,
                                   vector <float>& right,
                                   unsigned& leftCard,
                                   unsigned& rightCard,
                                   NumRange range)
{

    int i;
    Label label;
    pixelSet pSet;
    TrainImage* imgPtr;

    // Clear and initialize the vectors.
    left.clear();
    right.clear();
    left.resize(tp -> labelNum, 0.0f);
    right.resize(tp -> labelNum, 0.0f);

    // Initialize the cardinalities
    leftCard = 0;
    rightCard = 0;

    // Classify each pixel and put it into a subset
    for (i = range.start; i <= range.end; i++) {
       
        imgPtr = images -> getImgPtr((*td)[i].id);
        pSet = classifyPixel(phi, (*td)[i], (Image*)imgPtr);

        label = imgPtr -> getLabel((*td)[i].x, (*td)[i].y);
        
        switch (pSet) {
        
            // Put the classified pixel in the left subset.
            case LEFT:
                leftCard++;
                left[label - 1] += 1.0f;
                break;

            // Put the classified pixel in the right subset.
            case RIGHT:
                rightCard++;
                right[label - 1] += 1.0f;
                break;
        }
    }
    
    // Average to get the percentages
    for (i = 0; i < tp -> labelNum; i++) {
        if (leftCard != 0) left[i] /= leftCard; 
        if (rightCard != 0) right[i] /= rightCard; 
    }
}
 
/**
 *  Shannon Entropy function
 *
 *  @param probabilities is a vector that contains the percentage of
 *  each label occurrence in a set.
 *
 *  @return the entroply associated with this percentages.
 */
//CHECK
float RandomForest :: H (vector <float> probabilities)
{
    unsigned i;
    float entropy;

    entropy = 0.0f;
    for (i = 0; i < probabilities.size(); i++) {
        if (probabilities[i] == 0.0f) continue;
        entropy += probabilities[i] * log2 (probabilities[i]);   
    }

    return fabs(entropy);
}


/**
 *  This function determines if the given node stays as a leaf
 *  node or must be splitted in the training.
 *
 *  @param pointer to the node.
 *  @param range of the set.
 *  @param depth of the node.
 *  @param minimum number of examples per tree
 *
 *  @return true if the node must be a leaf or false in other case.
 */
 //CHECK
bool RandomForest :: testNode (Node **n,
                               Node *currentNode,
                               NumRange range)
{
    int i;
    int sampleCount;
    int depth;

    bool notEnoughSamples;
    bool depthReached;
    bool labeledEqual;

    Label firstLabel;
    Label nextLabel;

    // TODO: revisar si ese +1 no altero nada grave
    depth = getDepth((SplitNode *) currentNode) + 1;
    sampleCount = range.end - range.start + 1;

    notEnoughSamples = sampleCount <= tp -> minSampleCount;
    depthReached = depth >= tp -> maxDepth;

    labeledEqual = false;

    // If all labels are equal then return leaf node.
    firstLabel = images -> getLabel((*td)[range.start]);
    for (i = range.start + 1; i < range.end; i++ ) {
       
        nextLabel = images -> getLabel((*td)[i]);

        if (firstLabel != nextLabel) {
            labeledEqual = false;
            break;
        }
    }

    // All labels are equal
    if (i == range.end) {
        labeledEqual = true;
    }

    if (notEnoughSamples) {
        printf ("samples\n");
    }
    
    if (depthReached) {
        printf("depth reached\n");
    }

    if (labeledEqual) {
        printf("lab eq\n");
    }


    if (notEnoughSamples || depthReached || labeledEqual) {
        *n = (Node *) new LeafNode ();
        return LEAF;
    }
    else {
        *n = (Node *) new SplitNode ();
        return SPLIT;
    }

}

/**
 *  This function obtain the probability distribution of the
 *  labels in a range given of the train data pixels.
 *
 *  @param the range of pixels
 *  @param the train data set of pixels.
 *
 *  @return a vector with the propabilities
 */
 //CHECK
vector <float> RandomForest :: probDist (NumRange setRange)
{
    int i;
    float setSize;
    Label label;
    vector <float> percentages;

    // Obtain the number of elements in the set
    setSize = (setRange.end - setRange.start + 1);

    // Initialize percentage vector.
    percentages.resize(tp -> labelNum, 0.0f);
    
    // In case of errors.
    if (setSize == 0) {
        return percentages;
    }

    // Calculate the sum of the labels
    for (i = setRange.start; i <= setRange.end; i++) {
        label = images -> getLabel((*td)[i]);
        percentages[label - 1] += 1.0f;
    }
    
    // Calculate the percentage for each label
    for (i = 0; i < tp -> labelNum; i++) {
        percentages[i] /= setSize;
    }
    
    return percentages;

}

/**
 *  Gets the depth of a given node of a tree by the father
 *  chain.
 *
 *  @param node pointer
 *
 *  @return depth of the specified node
 */
 //CHECK
int RandomForest :: getDepth (Node *n)
{
    int depth;
    SplitNode *sNode = (SplitNode *) n;

    depth = 0;
    
    if (sNode != NULL) {
        while (sNode -> up != NULL) {
            depth ++;
            sNode = (SplitNode *) sNode -> up;
        }
    }

    return depth + 1;
}

/**
 *  classifyPixel
 *
 *  This function calculate the feature phi on the pixel x and
 *  depending on its threshold it will classify it as LEFT or
 *  RIGHT.
 *
 *  @param phi is te feacture to classify pixel.
 *  @param x is the pixel to be classifyed.
 *
 *  @return LEFT or RIGHT depending on the classification of the
 *  pixel x by the feature phi.
 */
 //CHECK
pixelSet RandomForest :: classifyPixel(SplitCandidate phi, 
                                       PixelInfo x, 
                                       Image *img)
{
    float featureValue;
    
    featureValue = calcFeature (phi.u, phi.v, x, img);
    if (featureValue < phi.t) {
        return LEFT;
    }
    else {
        return RIGHT;
    }
}

/**
 *  This function run the training of a single tree.
 *
 *  @param treeID of the tree to 
 *  @param maximum depth of the tree to construct.
 *  @param maximun number of pixels in a leaf node.
 *  @param number of pixels to select.
 *  @param number of training images.
 *  @param reference to the train data.
 */
 //CHECK
void RandomForest :: train (int treeID)
{
    unsigned nodeCount;

    int idx;
    int i;
    int done;
    int mpiSize;

    MPI_Status status;

    SplitCandidate bestSplit;
    SplitCandidate bestMPI;

    Node **root;
    Node *currentNode;
    Node *right;
    Node *left;

    bool nType;

    NumRange range;
    NumRange tmpRange;

    stack <Node *> nStack;
    stack <NumRange> trainIdx;

    // Get the number of processes in the MPI cluster
    MPI_Comm_size (MPI_COMM_WORLD, &mpiSize);

    nodeCount = 1;

    // Start with the root node.
    root = &trees[treeID];
   
    // Set initial range of training data.
    range.start = 0;
    range.end = td -> size() - 1;

    // Verifying type of node
    nType = testNode (root,
                      *root,
                      range);

    if (nType == SPLIT) {
        // Building split node
        ((SplitNode *) *root) -> up = NULL;
        nStack.push (*root);
        trainIdx.push (range);

        ((SplitNode *) *root) -> id = nodeCount;
        nodeCount++;
    } 
    else if (nType == LEAF) {
        
        // Calculate leaf probability distribution
        ((LeafNode *) *root) -> pDist = 
            probDist(range);
        
        ((LeafNode *) *root) -> id = nodeCount;
        nodeCount++;

    }
    
    while (!nStack.empty()) {
        // Obtain current node
        currentNode = nStack.top();
        nStack.pop();

        range = trainIdx.top();
        trainIdx.pop();


        tmpRange = range;

        done = 0;
        for (i = 1; i < mpiSize; i++) {

            MPI_Send(&done, 1, MPI_INT, i, 86, MPI_COMM_WORLD);

            // Send range to nodes
            MPI_Send(&range.start, 1, MPI_INT, i, 88, MPI_COMM_WORLD);
            MPI_Send(&range.end,   1, MPI_INT, i, 88, MPI_COMM_WORLD);
        }

        // Calculate best split in current node
        bestSplit = bestSplitThreadFun(range);


        // Receive and keep the best split candidate.
        for (i = 1; i < mpiSize; i++) {

            MPI_Recv(&bestMPI.u.x, 1, MPI_INT, i, 88, MPI_COMM_WORLD, &status);
            MPI_Recv(&bestMPI.u.y, 1, MPI_INT, i, 88, MPI_COMM_WORLD, &status);
            MPI_Recv(&bestMPI.v.x, 1, MPI_INT, i, 88, MPI_COMM_WORLD, &status);
            MPI_Recv(&bestMPI.v.y, 1, MPI_INT, i, 88, MPI_COMM_WORLD, &status);
            MPI_Recv(&bestMPI.t, 1, MPI_FLOAT, i, 88, MPI_COMM_WORLD, &status);
            MPI_Recv(&bestMPI.g, 1, MPI_FLOAT, i, 88, MPI_COMM_WORLD, &status);

            if (bestMPI.g > bestSplit.g) {
                bestSplit = bestMPI;
            }
        }

        // Send the best split candidate to all nodes
        for (i = 1; i < mpiSize; i++) {

            MPI_Send(&bestSplit.u.x, 1, MPI_INT, i, 88, MPI_COMM_WORLD);
            MPI_Send(&bestSplit.u.y, 1, MPI_INT, i, 88, MPI_COMM_WORLD);
            MPI_Send(&bestSplit.v.x, 1, MPI_INT, i, 88, MPI_COMM_WORLD);
            MPI_Send(&bestSplit.v.y, 1, MPI_INT, i, 88, MPI_COMM_WORLD);
            MPI_Send(&bestSplit.t, 1, MPI_FLOAT, i, 88, MPI_COMM_WORLD);
            MPI_Send(&bestSplit.g, 1, MPI_FLOAT, i, 88, MPI_COMM_WORLD);

        }

        // Sort train data given best split
        idx = sortData (range, bestSplit);

        // Temp range for the left node
        tmpRange.start = range.start;
        tmpRange.end = idx - 1;

        printf("rangos izq %d %d\n", tmpRange.start, tmpRange.end);

        nType = testNode (&left,
                          currentNode,
                          tmpRange);
    
        if (nType == SPLIT) {
            // Check left node

            ((SplitNode *) currentNode) -> left = left;

            // Build new node
            ((SplitNode *) left) -> up = currentNode;
            ((SplitNode *) left) -> left = NULL;
            ((SplitNode *) left) -> right = NULL;
            ((SplitNode *) left) -> phi = SplitCandidate();


            // Labeling node
            ((SplitNode *) left) -> id = nodeCount;
            nodeCount++;

            nStack.push (left);
            trainIdx.push (tmpRange);
        } 
        else if (nType == LEAF) {
            
            ((SplitNode *) currentNode) -> left = left;

            ((LeafNode *) left) -> pDist = 
                probDist(tmpRange);
                
            // Labeling node
            ((LeafNode *) left) -> id = nodeCount;
            nodeCount++;

        }

        tmpRange.start = idx;
        tmpRange.end = range.end;

        nType = testNode (&right, 
                          currentNode,
                          tmpRange);
    
        if (nType == SPLIT) {

            ((SplitNode *) currentNode) -> right = right;

            // Build new node
            ((SplitNode *) right) -> up = currentNode;
            ((SplitNode *) right) -> left = NULL;
            ((SplitNode *) right) -> right = NULL;
            ((SplitNode *) right) -> phi = SplitCandidate();
            
            // Labeling node
            ((SplitNode *) right) -> id = nodeCount;
            nodeCount++;

            nStack.push (right);
            trainIdx.push (tmpRange);
        } 
        else if (nType == LEAF) {
            
            ((SplitNode *) currentNode) -> right = right;
            
            ((LeafNode *) right) -> pDist = 
                probDist(tmpRange);
            
            // Labeling node
            ((LeafNode *) right) -> id = nodeCount;
            nodeCount++;


        }

        ((SplitNode *) currentNode) -> phi = bestSplit;
        
    }
}


/**
 *  This fuction visit all the nodes in a tree specified and
 *  print its content.
 *
 *  @param id of the tree.
 */
 //CHECK
void RandomForest :: traversal (int treeID)
{   
    printf ("TRAVERSAL TREE %d\n", treeID);
    unsigned int i;
    Node *root;
    Node *currentNode;
    Node *right;
    Node *left;
    
    stack <Node *> nStack;

    root = trees[treeID];
    nStack.push(root);

    while (!nStack.empty()) {
        currentNode = nStack.top();
        nStack.pop();

        
        // Push left element second
        left = ((SplitNode *) currentNode) -> left;

        if (left -> nodeType() != LEAF) {
            nStack.push(left);      
        }
        else {

            /*Imprimo lo de la hoja */

            printf ("probabilidades\n");
            for (i = 0; i < ((LeafNode *)left) -> pDist.size(); i++) {
                printf("%f ", ((LeafNode *)left) -> pDist[i]);
            }
            printf("\n");
        }
        
        // Push right element first
        right = ((SplitNode *) currentNode) -> right;

        if ( right -> nodeType() != LEAF) {
            nStack.push(right);
        }
        else {
            
            /* Imprimo lo de la hoja */
            printf ("probabilidades\n");
            for (i = 0; i < ((LeafNode *)right) -> pDist.size(); i++) {
                printf("%f ", ((LeafNode *)right) -> pDist[i]);
            }
            printf("\n");

        }
    }
}



/**
 *  This function classify a pixel of a given image by the
 *  random forest.
 *
 *  @param pointer to the image.
 *  @param pixel to classofy
 *  @param probability of the classification.
 *  @return label of the classification
 */
 //CHECK
Label RandomForest :: predict (Image* img, PixelInfo pixel, float& prob)
{

    int i;
    unsigned int j;
    Label maxLabel;
    float maxProb;
    float tmpProb;

    Node *currentNode;
    pixelSet ps;

    SplitNode *sNode;
    LeafNode *lNode;

    vector <float> postProb;
    postProb.resize (tp -> labelNum, 0.0f);

    for (i = 0; i < tp -> treeNum; i++ ) {
        currentNode = trees[i];
        
               
        // Drop pixel down the tree
        while ( (currentNode -> nodeType()) != LEAF) {
        
            sNode = (SplitNode *) currentNode;
            ps = classifyPixel (sNode -> phi, pixel, img);
        
            switch (ps) {
                case RIGHT:
                    currentNode = sNode -> right;
                    break;

                case LEFT:
                    currentNode = sNode -> left;
                    break;
                default:
                    printf("Error could not classify pixel\n");
                    break;
            }
        
        }
    
        lNode = (LeafNode *) currentNode;
        
        // Adds all the probabilities vector
        for (j = 0; j < lNode -> pDist.size(); j++) {
            postProb[j] += lNode -> pDist[j];
        }
    }
    
    maxProb = 0.0;
    tmpProb = 0.0;
    
    // Takes the label of the maximun probability
    for (j = 0; j < postProb.size(); j++) {
        tmpProb = postProb[j] / tp -> treeNum;

        if (tmpProb > maxProb) {
            maxProb = tmpProb;
            maxLabel = j;
        }
    }

    prob = maxProb;
    return maxLabel + 1;
}



/**
 *  This function start the training of the forest.
 *
 *  @param maximum depth of the tree.
 *  @param maximum number of examples to build a leaf node.
 *  @param number of pixels of an image for the training set.
 *  @param number of images per tree.
 *  @param number of offsets to generate.
 *  @param number of thresholds to generate.
 *
 */
 //CHECK
void RandomForest :: trainForest (trainParams& tparams)
{
    unsigned i;
    int j;
    int rank;
    int mpiSize;
    int done;
    int divFactor;
    int trainImgNum;
    int startIdx;
    int endIdx;
    MPI_Status status;

    tp = &tparams;
    
    vector <int> idxVec;

        // Load images from directory
    images = new ImagePool (tp -> imgDir);
    
    // Obtain the rank and the number of processes in the MPI cluster.
    MPI_Comm_rank (MPI_COMM_WORLD, &rank);
    MPI_Comm_size (MPI_COMM_WORLD, &mpiSize);

    // Calculate the division factor for offsets and threshold numbers
    //TODO: Acomodar la division de features
    divFactor = sqrt(mpiSize);
    tp -> offsetNum /= divFactor;
    tp -> thresholdNum /= divFactor;
    tp -> imgNum = images -> poolSize();

    idxVec = vector <int> (tp -> imgNum);
    
    if (rank == 0) {
        idxVec = permutation(tp -> imgNum);
        
        // Sending ordering of the pool.
        for (j = 1; j < mpiSize; j++) {
            for (i = 0; i < idxVec.size(); i++) {    
                MPI_Send(&idxVec[i], 1, MPI_INT, j, 86, MPI_COMM_WORLD);
            }
        }
    }
    else { 
    
        // Receive pool reordering.
        for (i = 0; i < idxVec.size(); i++) {    
            MPI_Recv(&idxVec[i], 1, MPI_INT, 0, 86, MPI_COMM_WORLD, &status);
        }
    
    }

    
    images -> poolReorder(idxVec);

    trees.resize(tp -> treeNum, NULL);
    trainImgNum = tp -> imgNum / tp -> treeNum;

    startIdx = 0;
    endIdx = 0;
    //trainImgNum = tp -> trainImgNum;

    //trainImgNum = 12;

    for (i = 0; i < trees.size(); i++) {

        // Setting the range of images which every tree is going to
        // work with

        // TODO: es probable que el ultimo arbol entrene con mas 
        // imagenes que los anteriores, seria bueno ver como arreglar
        // eso

        if (i != unsigned(tp -> treeNum - 1)) {
            startIdx = i * trainImgNum;
            endIdx = startIdx + trainImgNum - 1;
        }
        else{
            startIdx = i * trainImgNum;
            endIdx = images -> poolSize() - 1;
        }
        
        cout << "ARBOL " << i << endl;
        cout << "INDICE START " << startIdx << endl;
        cout << "INDICE END " << endIdx << endl;


        //startIdx = 0;
        //endIdx = images -> poolSize() -1;
        // Only the master process initialize the train data.
        //TODO: recordar la forma de samplear los pixel (true para que
        //sea por label.
        if (rank == 0) {
            td = new TrainData(tp -> samplePixelNum, *images, 
                               startIdx, endIdx, false);
        }
        else {
            td = new TrainData(endIdx - startIdx + 1, tp -> samplePixelNum);
        }

        // Synchronize the train data
        for (j = 0; j < td -> size(); j++) {
            MPI_Bcast(&(*td)[j].id, 1, MPI_UNSIGNED_SHORT, 0, MPI_COMM_WORLD);
            MPI_Bcast(&(*td)[j].x, 1, MPI_UNSIGNED_SHORT, 0, MPI_COMM_WORLD);
            MPI_Bcast(&(*td)[j].y, 1, MPI_UNSIGNED_SHORT, 0, MPI_COMM_WORLD);
        } 

        if (rank == 0) {

            train(i);

            done = 42;

            for (j = 1; j < mpiSize; j++) {
                MPI_Send(&done, 1, MPI_INT, j, 86, MPI_COMM_WORLD);
            }
        }
        else {

            NumRange range;
            SplitCandidate bestSplit;

            done = 0;
            while (!done) {

                // check to continue
                MPI_Recv(&done, 1, MPI_INT, 0, 86, MPI_COMM_WORLD, &status);

                if (done) { break; }

                // Recive the range in where to look for a split candidate.
                MPI_Recv(&range.start, 1, MPI_INT, 0, 88, MPI_COMM_WORLD, &status);
                MPI_Recv(&range.end,   1, MPI_INT, 0, 88, MPI_COMM_WORLD, &status);

                bestSplit = bestSplitThreadFun(range);

                // Send the best split generated.
                MPI_Send(&bestSplit.u.x, 1, MPI_INT, 0, 88, MPI_COMM_WORLD);
                MPI_Send(&bestSplit.u.y, 1, MPI_INT, 0, 88, MPI_COMM_WORLD);
                MPI_Send(&bestSplit.v.x, 1, MPI_INT, 0, 88, MPI_COMM_WORLD);
                MPI_Send(&bestSplit.v.y, 1, MPI_INT, 0, 88, MPI_COMM_WORLD);
                MPI_Send(&bestSplit.t, 1, MPI_FLOAT, 0, 88, MPI_COMM_WORLD);
                MPI_Send(&bestSplit.g, 1, MPI_FLOAT, 0, 88, MPI_COMM_WORLD);

                // Recive the best of the total.
                MPI_Recv(&bestSplit.u.x, 1, MPI_INT, 0, 88, MPI_COMM_WORLD, &status);
                MPI_Recv(&bestSplit.u.y, 1, MPI_INT, 0, 88, MPI_COMM_WORLD, &status);
                MPI_Recv(&bestSplit.v.x, 1, MPI_INT, 0, 88, MPI_COMM_WORLD, &status);
                MPI_Recv(&bestSplit.v.y, 1, MPI_INT, 0, 88, MPI_COMM_WORLD, &status);
                MPI_Recv(&bestSplit.t, 1, MPI_FLOAT, 0, 88, MPI_COMM_WORLD, &status);
                MPI_Recv(&bestSplit.g, 1, MPI_FLOAT, 0, 88, MPI_COMM_WORLD, &status);

                // Sort the data with the best split candidate
                sortData (range, bestSplit);

            }
        }
    }
}

/**
 *  writeNodeToFile
 *
 *  This function write the information of a node to a file.
 *
 *  @param pointer to the node.
 *  @param file pointer to the file.
 */
 //CHECK
void RandomForest :: writeNodeToFile (Node *currentNode,
                                      FILE *fp)
{   
    unsigned int i;
    int nodeID;
    
    SplitCandidate phi;

    if (currentNode -> nodeType() != LEAF) {

        // Setting node parameters to write
        nodeID = ((SplitNode *) currentNode) -> id;
        phi = ((SplitNode *) currentNode) -> phi;

        fprintf (fp, "S %d %d %d %d %d %f\n", nodeID, phi.u.x, 
            phi.u.y, phi.v.x, phi.v.y, phi.t);
    }
    else {

        // Setting node parameters to write
        nodeID = ((LeafNode *) currentNode) -> id;

        fprintf (fp, "L %d ", nodeID);

        for (i = 0; i < ((LeafNode *)currentNode) -> pDist.size(); i++){
            fprintf (fp, "%f ", ((LeafNode *) currentNode) -> pDist[i]);
        }
        fprintf (fp, "\n");
    }
}

/**
 *  writeTreeToFile
 *
 *  Write a trained tree to a file.
 * 
 *  @param treeID is the id of the trained tree to write into a
 *  file.
 *  @param fileName is the string containing the path to the
 *  output file.
 *
 */
 //CHECK
void RandomForest :: writeTreeToFile(int treeID, string fileName)
{
    FILE* fp;

    Node *root;
    Node *currentNode;
    Node *right;
    Node *left;

    SplitCandidate phi;
    
    stack <Node *> nStack;

    if ((fp = fopen(fileName.c_str(), "w")) == NULL) {
        printf("Cannot open file %s.\n", fileName.c_str());
        exit(1);
    }
    else {
        printf ("Saving tree %d, into file %s\n", treeID, fileName.c_str());
    }
    
    root = trees[treeID];
    
    writeNodeToFile (root, fp);
    
    //TODO: se cambio por la funcion previa. Probar
    /*if (root -> nodeType() != LEAF) {
        
        // Setting node parameters to write
        nodeID = ((SplitNode *)root) -> id;
        phi = ((SplitNode *)root) -> phi;

        fprintf (fp, "S %d %d %d %d %d %f\n", nodeID, 
            phi.u.x, phi.u.y, phi.v.x, phi.v.y, phi.t);
        //nStack.push(((SplitNode *)root) -> left);
        //nStack.push(((SplitNode *)root) -> right);
    }
    else {
 
        // Setting node parameters to write
        nodeID = ((LeafNode *) root) -> id;

        fprintf (fp, "L %d ", nodeID);

        for (i = 0; i < ((LeafNode *)root) -> pDist.size(); i++){
            fprintf (fp, "%f ", ((LeafNode *)root) -> pDist[i]);
        }
        fprintf (fp, "\n");   

    }
    */
    if (root -> nodeType() != LEAF) {
        nStack.push(root);
    }

    while (!nStack.empty()) {
        
        currentNode = nStack.top();
        nStack.pop();
        
        left = ((SplitNode *) currentNode) -> left;
        writeNodeToFile (left, fp);
        
        if (left -> nodeType() != LEAF) {
            nStack.push(left);
        }

        right = ((SplitNode *) currentNode) -> right;
        writeNodeToFile (right, fp);
        
        if (right -> nodeType() != LEAF) {
            nStack.push(right);      
        }
    }
    fclose(fp);
}

/**
 *  Write the traided trees to diferent text files in a
 *  directory especified. The trees are saved in files named
 *  "i.tree".
 *
 *  @param path to the directory
 */
 // CHECK
void RandomForest :: writeForest(string dirName)
{
    unsigned i;
    stringstream fileName;
    for (i = 0; i < trees.size(); i++) {
        fileName << dirName << "/" << i << ".tree";
        writeTreeToFile(i,fileName.str());
        fileName.flush();
        fileName.str("");
    }
}


/**
 *  loadNodeFromFile
 *
 *  This function loads the information of a node.
 *
 *  @param pointer to the node.
 *  @param file pointer to the file.
 */
 //CHECK
void RandomForest :: loadNodeFromFile (Node **currentNode,
                                       Node **sideNode,
                                       char nodeType,
                                       int nodeID,
                                       int side,
                                       stack <Node *> *nStack,
                                       FILE *fp)
{
    unsigned i;

    SplitCandidate phi;

    vector <float> probs;
    probs.resize(tp -> labelNum, 0.0);

    // Load depending of type (split, leaf)
    if (nodeType == 'S') {

        fscanf (fp, "%d %d %d %d %f\n", &phi.u.x, 
                &phi.u.y, &phi.v.x, &phi.v.y, &phi.t);

        *sideNode = new SplitNode (phi);

        ((SplitNode*) *sideNode) -> id = nodeID;
        if (currentNode != NULL) {
            ((SplitNode*) *sideNode) -> up = *currentNode;
        }
        else {
            ((SplitNode*) *sideNode) -> up = NULL;
        }

        // Push the node into stack
        nStack -> push (*sideNode);
    }
    else if (nodeType == 'L') {
        
        // Filling probabilities vector
        for (i = 0; i < probs.size(); i++){
            fscanf (fp, "%f ", &probs[i]);
        }

        fscanf (fp, "\n");   

        // Building node
        *sideNode = new LeafNode (probs);
        ((LeafNode *) *sideNode) -> id = nodeID;
    }


    // Build depending the side of the node
    if (side == LEFT) {
        ((SplitNode *) *currentNode) -> left = *sideNode;
    }
    else if (side == RIGHT) {
        ((SplitNode *) *currentNode) -> right = *sideNode;
    }

}


/**
 *  loadTreeFromFile
 *
 *  load a trained tree from a file.
 * 
 *  @param fileName is the string containing the path to the file.
 *
 */
 // CHECK
void RandomForest :: loadTreeFromFile(string fileName)
{
    FILE* fp;
    int nodeID;

    char nodeType;

    vector <float> probs;

    Node *root = NULL;
    Node *currentNode = NULL;
    Node *right = NULL;
    Node *left = NULL;

    SplitCandidate phi;

    stack <Node *> nStack;

    probs.resize(tp -> labelNum, 0.0);

    if ((fp = fopen(fileName.c_str(), "r")) == NULL) {
        printf("Cannot open file %s.\n", fileName.c_str());
        exit(1);
    }
    else {
        printf ("Loading tree from file %s\n", fileName.c_str());
    }
    
    // Building root node
    fscanf (fp, "%c %d ", &nodeType, &nodeID);

    loadNodeFromFile (NULL, &root, nodeType, nodeID, -1, &nStack, fp);
    
    // Building tree
    while (fscanf (fp, "%c %d ", &nodeType, &nodeID) > 0) {
        
        currentNode = nStack.top();
        nStack.pop();
        
        loadNodeFromFile (&currentNode, &left, nodeType, nodeID, LEFT,
                            &nStack, fp);
        
        fscanf (fp, "%c %d ", &nodeType, &nodeID);

        loadNodeFromFile (&currentNode, &right, nodeType, nodeID, RIGHT,
                            &nStack, fp);
    }

    trees.push_back(root);
    fclose(fp);
}



/**
 *  Load the trees contained in a directory with the name
 *  "i.tree".
 *
 *  @param path to the directory
 */
 //CHECK
void RandomForest :: loadForest(int numTrees, 
                                int numLabels, 
                                string dirName)
{
    int i;
    stringstream fileName;

    tp = new trainParams();
    tp -> treeNum = numTrees;
    tp -> labelNum = numLabels;


    for (i = 0; i < numTrees; i++) {
        fileName << dirName << "/" << i << ".tree";
        loadTreeFromFile(fileName.str());
        fileName.flush();
        fileName.str("");
    }
}

/**
 * Determines the percentage of pixels classified of an image.
 * @param img Input image.
 * @return Percentage of pixels correctly classified.
 */
 //CHECK
float RandomForest :: testClassification (TrainImage& img) 
{   
    int i;
    int j;
    
    int noNullPixels;
    int positive;

    float prob;

    Label labelCalc;
    Label label;
    
    PixelInfo pix;

    positive = 0;
    noNullPixels = 0;

    for (i = 0; i < img.height; i++) {
        for (j = 0; j < img.width; j++) {
            pix = PixelInfo (i, j);
            label = img.getLabel(i,j);
            
            if (label > 0) {
                noNullPixels++;
                labelCalc = predict ((Image*)&img, pix, prob);
                if (label == labelCalc) {
                    
                    positive++;
                }
            }
        }
    }

    return (float)positive / (float)noNullPixels;
}



/**
 * Determines the percentage of pixels classified of an image and
 * print the classification to a image output file.
 * @param img Input image.
 * @return Percentage of pixels correctly classified.
 */
 //CHECK
float RandomForest :: testClassificationImage (TrainImage& img, 
                                               string imgName) 
{   
    int i;
    int j;
    
    int noNullPixels;
    int positive;

    float prob;
    

    Label labelCalc;
    Label label;
    
    PixelInfo pix;

    // Image formation
    IplImage* outImg;
    stringstream fileBMP;

    positive = 0;
    noNullPixels = 0;
    
    outImg = cvCreateImage(cvSize(img.width, img.height),IPL_DEPTH_8U,3);


    for (i = 0; i < img.height; i++) {
        for (j = 0; j < img.width; j++) {
            pix = PixelInfo (i, j);
            label = img.getLabel(i,j);
            
            if (label > 0) {
                noNullPixels++;
                labelCalc = predict ((Image*)&img, pix, prob);
                if (label == labelCalc) {
                    switch(label) {
                        case 1:
                            cvSet2D(outImg, i, j, CV_RGB(255,0,0));
                            break;
                        case 2:
                            cvSet2D(outImg, i, j, CV_RGB(0,255,0));
                            break;
                        case 3:
                            cvSet2D(outImg, i, j, CV_RGB(0,0,255));
                            break;
                        case 4:
                            cvSet2D(outImg, i, j, CV_RGB(255,255,0));
                            break;
                        case 5:
                            cvSet2D(outImg, i, j, CV_RGB(0,255,255));
                            break;
                        case 6:
                            cvSet2D(outImg, i, j, CV_RGB(255,0,255));
                            break;
                    }
                    positive++;
                    //cout << "Clasificacion bien " << prob<<"label " << (int)label << endl;
                /*
                } else {
                    
                    switch(labelCalc) {
                        case 1:
                            cvSet2D(outImg, i, j, CV_RGB(255,0,0));
                            break;
                        case 2:
                            cvSet2D(outImg, i, j, CV_RGB(0,255,0));
                            break;
                        case 3:
                            cvSet2D(outImg, i, j, CV_RGB(0,0,255));
                            break;
                        case 4:
                            cvSet2D(outImg, i, j, CV_RGB(255,255,0));
                            break;
                        case 5:
                            cvSet2D(outImg, i, j, CV_RGB(0,255,255));
                            break;
                        case 6:
                            cvSet2D(outImg, i, j, CV_RGB(255,0,255));
                            break;
                    }
                    */
                    //cvSet2D(outImg, i, j, CV_RGB(255,255,255));
                    //cout << "Clasificacion mal " << prob<<"label " << (int)label << endl;
                }
            }
        }
    }
    
    fileBMP <<"out_images/"<<imgName << ".bmp";

    cvSaveImage((fileBMP.str()).c_str(), outImg);

    fileBMP.flush();
    fileBMP.str("");

    return (float)positive / (float)noNullPixels;
}