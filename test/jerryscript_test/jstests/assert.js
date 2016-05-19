// Provide the assert function needed for running tests.
function assert (condition)
{
    if (!condition)
    {
        print ("Script assertion failed");
        throw '';
    }
}
