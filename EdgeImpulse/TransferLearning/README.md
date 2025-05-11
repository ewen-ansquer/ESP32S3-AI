# Object Detection
## Choosing a labeling method
Set labelling method to 'One label per data image'

![labelling method](../labeling_method.png)

## Adding data
To add data, click on ‘Add existing data’.

Then click on "Upload data".

A menu will open, making it easier to upload a folder.

You can choose the label of an item based on its name, or enter it into the whole collection uploaded. 
Example: `Apple.1.jpeg` will add the item `Apple.1` to the Apple class.

Next, if you have not planned to separate your training data from your testing data, then click on 'Automatically split between training and testing'. This will randomly split your data with an 80%/20% ratio.

> [!TIP]
> You can also have 'train' or 'training' and 'test' or 'testing' in the parent folder, allowing you to split your data when uploading.

![add_data](add_data.png)

## Creating your impulse

Each project is based around an impulse (at least).
An impulse is an experience, and you can find it under this term. It describes the path to a trained model. It is made up of 3 blocks:
1. Input block: This describes how your images are resized.
2. Processing block: This is a feature extractor and will also normalise the data so that it can be used in the training phase.
3. Learning block: The training phase...

Using more impulses (up to 3 with the free plan) will let you compare different results.

![impulse](impulse.png)

For our project, you want an Image processing block and an Object Detection processing block.

## Processing block
It is a normalizer that will convert your image to RGB or greyscale format.

![image parameters](image_parameters.png)

Once you choose your image color depth, you have to save the parameters. Next, click on 'Generate features'. After a moment, you'll get a graph. The farther points from the different classes are, the better it is. 

## Learning block (Training)

1. Choose the number of training cycles (aka 'Epoch')
2. Choose the learning rate.
3. Check data augmentation if you have not transformed your dataset (with noise, for example)
4. Choose your model: MobilNetV2 160x160 1.0 (no final layer, 0.1 dropout). The dropout corresponds to the rate of neurons disabled at each epoch.

> [!IMPORTANT]
> We used the default values (20 for the number of training cycles, 0.0005 for the learning rate). If you want to reduce the number of training cycles, do not forget to increase the learning rate.

![training](transfer_learning.png)

## Building project
1. Search for Arduino Library.
2. Be sure to select the TensorFlow Lite compiler (or your model will not work).
3. Select Quantized (int8), to have a smaller and faster model.
4. Build

![build](build.png)


You can go back to the parent folder "Edge Impulse".