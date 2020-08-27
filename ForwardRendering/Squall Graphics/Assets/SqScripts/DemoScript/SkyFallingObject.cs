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
    public int objectCount = 50;

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
    public float maxRange = 200f;

    /// <summary>
    /// fall speed
    /// </summary>
    public float fallSpeed = 5f;

    GameObject[] objPool;
    System.Random rng;
    Transform mainCamTrans;
    Transform transformCache;

    void Start()
    {
        if (!targetObj)
        {
            return;
        }

        transform.position = Vector3.zero;
        transform.rotation = Quaternion.identity;
        rng = new System.Random(Guid.NewGuid().GetHashCode());
        mainCamTrans = Camera.main.transform;

        objPool = new GameObject[objectCount];
        for (int i = 0; i < objectCount; i++)
        {
            objPool[i] = Instantiate(targetObj);
            objPool[i].transform.SetParent(transform);

            float sign_x = (rng.Next() & 1) == 0 ? 1f : -1f;
            float sign_z = (rng.Next() & 1) == 0 ? 1f : -1f;
            float rx = (float)rng.NextDouble() * maxRange * sign_x;
            float rz = (float)rng.NextDouble() * maxRange * sign_z;

            objPool[i].transform.position = new Vector3(rx, initHeight, rz) + mainCamTrans.position;
            objPool[i].transform.rotation = Quaternion.Euler(rng.Next(-180, 180), rng.Next(-60, 60), 0);
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
            transformCache.SetPositionAndRotation(new Vector3(0, initHeight, 0) + mainCamTrans.position, Quaternion.identity);
        }
    }
}
