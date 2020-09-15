using System;
using UnityEngine;

/// <summary>
/// sky falling object
/// </summary>
public class SkyFallingObject : MonoBehaviour
{
    /// <summary>
    /// target obj
    /// </summary>
    public GameObject targetObj;

    /// <summary>
    /// object count
    /// </summary>
    public int lightGap = 20;

    /// <summary>
    /// init height
    /// </summary>
    public float initHeight = 20f;

    /// <summary>
    /// reset height
    /// </summary>
    public float resetHeight = -20f;

    /// <summary>
    /// max range
    /// </summary>
    public int maxRange = 200;

    /// <summary>
    /// fall speed
    /// </summary>
    public float fallSpeed = 5f;

    GameObject[] objPool;
    System.Random rng;
    Transform mainCamTrans;
    Transform transformCache;

    void Awake()
    {
        if (!targetObj)
        {
            return;
        }

        transform.position = Vector3.zero;
        transform.rotation = Quaternion.identity;
        rng = new System.Random(Guid.NewGuid().GetHashCode());
        mainCamTrans = Camera.main.transform;

        int lightRowCount = maxRange / lightGap;
        objPool = new GameObject[lightRowCount * lightRowCount];

        for (int i = 0; i < lightRowCount; i++)
        {
            for (int j = 0; j < lightRowCount; j++)
            {
                int idx = j + i * lightRowCount;
                objPool[idx] = Instantiate(targetObj);
                objPool[idx].transform.SetParent(transform);

                float rx = -maxRange / 2 + i * lightGap;
                float rz = -maxRange / 2 + j * lightGap;

                objPool[idx].transform.position = new Vector3(rx, initHeight, rz) + mainCamTrans.position;
            }
        }

        transformCache = transform;
        transformCache.position = mainCamTrans.position + Vector3.one * initHeight;
    }

    // Update is called once per frame
    void Update()
    {
        Vector3 pos = transformCache.position;
        pos.y -= fallSpeed * Time.deltaTime;
        transformCache.position = pos;

        if (pos.y < resetHeight)
        {
            pos.y = initHeight;
            transformCache.position = pos;
        }
    }
}
