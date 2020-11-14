using UnityEngine;

/// <summary>
/// simple rotate light
/// </summary>
public class RotateLight : MonoBehaviour
{
    /// <summary>
    /// rotate speed
    /// </summary>
    public float rotateSpeed = 10f;

    /// <summary>
    /// rotate axis
    /// </summary>
    public Vector3 rotateAxis = Vector3.up;

    void Update()
    {
        transform.Rotate(rotateAxis, rotateSpeed * Time.deltaTime, Space.World);
    }
}
