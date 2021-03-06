import sys
import os
import json
import ase
import argparse
from numpy.polynomial.legendre import leggauss
import numpy as np
import ubjson

root = os.path.abspath("../")
rascal_reference_path = os.path.join(root, "reference_data/")
inputs_path = os.path.join(rascal_reference_path, "inputs")
dump_path = os.path.join("reference_data/", "tests_only")


# Computes the sample points and weights for Gauss-Legendre quadrature and rescales them.
def get_leggauss(order, a, b):
    x, w = leggauss(order)
    # rescaling
    x = (b - a) * 0.5 * x + 0.5 * (a + b)
    w = (b - a) * 0.5 * w
    return x, w


def dump_reference_json():
    sys.path.insert(0, os.path.join(root, "build/"))
    sys.path.insert(0, os.path.join(root, "tests/"))
    data = []
    a = 0
    for order in range(2, 20):
        for b in np.linspace(2, 10, 20):
            x, w = get_leggauss(order, a, b)
            data.append(
                dict(
                    a=a,
                    b=b,
                    order=order,
                    points=x.tolist(),
                    weights=w.tolist(),
                )
            )
    print(len(data))
    with open(
        os.path.join(root, dump_path, "gauss_legendre_reference.ubjson"), "wb"
    ) as f:
        ubjson.dump(data, f)


##########################################################################################
##########################################################################################


def main(json_dump):
    if json_dump == True:
        dump_reference_json()


##########################################################################################
##########################################################################################


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-json_dump", action="store_true", help="Switch for dumping json"
    )

    args = parser.parse_args()
    main(args.json_dump)
