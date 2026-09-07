#!/usr/bin/env bash
set -euo pipefail

readonly version="v3.8.0"
readonly archive_url="https://gitlab.com/libtiff/libtiff-pics/-/archive/${version}/libtiff-pics-${version}.tar.gz"
readonly lfs_batch_url="https://gitlab.com/libtiff/libtiff-pics.git/info/lfs/objects/batch"
readonly project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly destination="${project_dir}/sample_tiff"

mkdir -p "${destination}"

temp_dir="$(mktemp -d)"
trap 'rm -rf -- "${temp_dir}"' EXIT

archive="${temp_dir}/libtiff-pics.tar.gz"
echo "Downloading libtiff-pics ${version}..."
curl --fail --location --retry 3 --output "${archive}" "${archive_url}"
tar -xzf "${archive}" -C "${temp_dir}"

source_dir="$(find "${temp_dir}" -mindepth 1 -maxdepth 1 -type d -name 'libtiff-pics-*' -print -quit)"
if [[ -z "${source_dir}" ]]; then
    echo "Cannot find the extracted libtiff-pics directory" >&2
    exit 1
fi

copied=0
skipped=0
while IFS= read -r -d '' source_file; do
    relative_path="${source_file#"${source_dir}/"}"
    destination_file="${destination}/${relative_path}"
    mkdir -p "$(dirname "${destination_file}")"

    if [[ -e "${destination_file}" ]]; then
        skipped=$((skipped + 1))
        continue
    fi

    file_to_copy="${source_file}"
    if head -c 64 "${source_file}" | grep -q 'git-lfs.github.com/spec'; then
        oid="$(sed -n 's/^oid sha256://p' "${source_file}")"
        size="$(sed -n 's/^size //p' "${source_file}")"
        if [[ ! "${oid}" =~ ^[0-9a-f]{64}$ || ! "${size}" =~ ^[0-9]+$ ]]; then
            echo "Invalid Git LFS pointer: ${relative_path}" >&2
            exit 1
        fi

        response_file="${temp_dir}/lfs-response.json"
        curl --fail --silent --show-error \
            -H 'Accept: application/vnd.git-lfs+json' \
            -H 'Content-Type: application/vnd.git-lfs+json' \
            --data "{\"operation\":\"download\",\"transfers\":[\"basic\"],\"objects\":[{\"oid\":\"${oid}\",\"size\":${size}}]}" \
            --output "${response_file}" "${lfs_batch_url}"
        download_url="$(jq -er '.objects[0].actions.download.href' "${response_file}")"
        file_to_copy="${temp_dir}/${oid}"
        curl --fail --location --retry 3 --silent --show-error \
            --output "${file_to_copy}" "${download_url}"

        actual_oid="$(shasum -a 256 "${file_to_copy}" | awk '{print $1}')"
        if [[ "${actual_oid}" != "${oid}" ]]; then
            echo "Checksum mismatch for ${relative_path}" >&2
            exit 1
        fi
    fi

    cp "${file_to_copy}" "${destination_file}"
    copied=$((copied + 1))
done < <(find "${source_dir}" -type f \( -iname '*.tif' -o -iname '*.tiff' -o -iname '*.g3' \) -print0)

echo "Done: copied ${copied}, kept existing ${skipped}."
echo "Samples are in ${destination}"
